#include <algorithm>
#include <numeric>
#include <string.h>
#include <tracy/Tracy.hpp>
#include <vector>

#include "VlkStagingRing.hpp"

#ifdef TRACY_ENABLE
#  include <tracy/TracyVulkan.hpp>
#endif

#include "util/Panic.hpp"
#include "vulkan/VlkBuffer.hpp"
#include "vulkan/VlkDevice.hpp"
#include "vulkan/VlkFence.hpp"
#include "vulkan/VlkPhysicalDevice.hpp"
#include "vulkan/VlkQueueType.hpp"

namespace
{
constexpr uint32_t stripSizeDivisor = 4;

struct Strip
{
    uint32_t firstRow;
    uint32_t numRows;
};

[[nodiscard]] uint64_t AlignUp( uint64_t value, uint64_t alignment )
{
    const uint64_t rem = value % alignment;
    return rem != 0 ? value + alignment - rem : value;
}

[[nodiscard]] std::vector<Strip> SplitLevel( uint64_t rowBytes, uint32_t height, uint64_t ringBytes, uint32_t divisor, uint32_t granY )
{
    uint64_t stripRows = ( ( ringBytes / divisor ) / rowBytes ) / granY * granY;
    if( stripRows < granY ) stripRows = granY;
    if( stripRows > height ) stripRows = height;

    std::vector<Strip> strips;
    for( uint32_t y = 0; y < height; )
    {
        const uint32_t n = (uint32_t)( stripRows < uint64_t( height - y ) ? stripRows : uint64_t( height - y ) );
        strips.emplace_back( Strip { y, n } );
        y += n;
    }
    return strips;
}
} // namespace

VlkStagingRing::VlkStagingRing( VlkDevice& device, VkDeviceSize size )
    : m_device( device )
    , m_cmds( *device.GetCommandPool( QueueType::Transfer ), 16 )
{
    const auto& props = device.GetPhysicalDevice()->Properties();
    m_granularity = std::max( props.limits.bufferImageGranularity, uint64_t( 1 ) );
    m_atom = std::max( props.limits.nonCoherentAtomSize, uint64_t( 1 ) );
    m_transferGranY = std::max( device.GetPhysicalDevice()->QueueFamilyProperties()[ device.GetQueueInfo( QueueType::Transfer ).idx ].minImageTransferGranularity.height, 1u );

    m_size = AlignUp( size, m_granularity );

    const VkBufferCreateInfo bufInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = m_size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    m_buffer = std::make_shared<VlkBuffer>( device, bufInfo, VlkBuffer::WillWrite | VlkBuffer::PreferHost );
}

VlkStagingRing::~VlkStagingRing()
{
    std::lock_guard lock( m_lock );
    while( !m_inFlight.empty() ) RetireOldest();
}

void VlkStagingRing::Level( VkImage image, uint32_t level, uint32_t width, uint32_t height, uint32_t bpp,
                            const void* src, const std::function<void( VkCommandBuffer )>& levelBarrier )
{
    ZoneScoped;

    const uint64_t rowBytes = uint64_t( width ) * bpp;
    CheckPanic( rowBytes <= m_size, "Mip row is larger than the staging ring" );

    const auto strips = SplitLevel( rowBytes, height, m_size, stripSizeDivisor, m_transferGranY );
    const auto* base = (const uint8_t*)src;
    auto* dst = (uint8_t*)m_buffer->Ptr();
    VkBuffer staging = *m_buffer;
    bool first = true;

    for( const auto& strip : strips )
    {
        const uint64_t bytes = uint64_t( strip.numRows ) * rowBytes;

        std::lock_guard lock( m_lock );

        NeedRegion( bytes, std::lcm( m_granularity, uint64_t( bpp ) ) );
        const uint64_t offset = m_cursor;
        m_cursor += bytes;

        memcpy( dst + offset, base + uint64_t( strip.firstRow ) * rowBytes, bytes );

        VkCommandBuffer cmdBuf = m_cmds.Acquire();
#ifdef TRACY_ENABLE
        {
            static constexpr tracy::SourceLocationData stagingZone { "Staging submit", TracyFunction, TracyFile, (uint32_t)TracyLine, 0 };
            tracy::VkCtxScope gpuZone( m_device.GetTracyContext(), &stagingZone, cmdBuf, true );
#endif
            if( first )
            {
                levelBarrier( cmdBuf );
                first = false;
            }

            const VkBufferImageCopy region = {
                .bufferOffset = offset,
                .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, level, 0, 1 },
                .imageOffset = { 0, int32_t( strip.firstRow ), 0 },
                .imageExtent = { width, strip.numRows, 1 }
            };
            vkCmdCopyBufferToImage( cmdBuf, staging, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );
#ifdef TRACY_ENABLE
        }
#endif
        m_cmds.End( cmdBuf );
        const uint64_t flushBegin = offset - ( offset % m_atom );
        const uint64_t flushEnd = std::min( offset + bytes + ( ( m_atom - ( offset + bytes ) % m_atom ) % m_atom ), m_size );
        m_buffer->Flush( flushBegin, flushEnd - flushBegin );

        auto fence = std::make_shared<VlkFence>( m_device );
        m_device.Submit( cmdBuf, QueueType::Transfer, *fence );
        m_inFlight.emplace_back( InFlight { std::move( fence ), cmdBuf, offset, offset + bytes } );

        while( m_inFlight.size() > stripSizeDivisor * 4 && m_inFlight.front().fence->Wait( 0 ) == VK_SUCCESS )
        {
            auto done = std::move( m_inFlight.front() );
            m_inFlight.pop_front();
            m_cmds.Release( done.cmdBuf );
        }
        while( m_inFlight.size() > stripSizeDivisor * 16 ) RetireOldest();
    }
}

void VlkStagingRing::NeedRegion( uint64_t bytes, uint64_t alignment )
{
    m_cursor = AlignUp( m_cursor, alignment );
    if( m_cursor + bytes > m_size )
    {
        m_cursor = 0;
    }

    for(;;)
    {
        auto it = std::find_if( m_inFlight.begin(), m_inFlight.end(),
            [&]( const InFlight& f ) { return f.begin < m_cursor + bytes && m_cursor < f.end; } );
        if( it == m_inFlight.end() ) break;
        it->fence->Wait();
        auto cmdBuf = it->cmdBuf;
        m_inFlight.erase( it );
        m_cmds.Release( cmdBuf );
    }

    CheckPanic( m_cursor + bytes <= m_size, "Staging region exceeds ring size" );
}

void VlkStagingRing::RetireOldest()
{
    ZoneScopedN( "Staging fence wait" );

    auto entry = std::move( m_inFlight.front() );
    m_inFlight.pop_front();

    entry.fence->Wait();
    m_cmds.Release( entry.cmdBuf );
}
