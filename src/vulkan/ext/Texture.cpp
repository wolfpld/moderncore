#include <algorithm>
#include <cmath>
#include <string.h>
#include <vulkan/vulkan.h>
#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>

#include "Texture.hpp"
#include "util/Bitmap.hpp"
#include "vulkan/VlkBuffer.hpp"
#include "vulkan/VlkCommandBuffer.hpp"
#include "vulkan/VlkDevice.hpp"
#include "vulkan/VlkFence.hpp"
#include "vulkan/VlkGarbage.hpp"
#include "vulkan/ext/Tracy.hpp"

struct MipData
{
    uint32_t width;
    uint32_t height;
    uint64_t offset;
    uint64_t size;
};

static inline VkImageCreateInfo GetImageCreateInfo( VkFormat format, uint32_t width, uint32_t height, uint32_t mipLevels )
{
    return {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = { width, height, 1 },
        .mipLevels = mipLevels,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
}

static inline VkImageViewCreateInfo GetImageViewCreateInfo( VkImage image, VkFormat format, uint32_t mipLevels )
{
    return {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1 }
    };
}

static inline VkBufferCreateInfo GetStagingBufferInfo( uint64_t size )
{
    return {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
}

Texture::Texture( VlkDevice& device, const Bitmap& bitmap, VkFormat format, bool mips, std::vector<std::shared_ptr<VlkFence>>& fencesOut, TaskDispatch* td )
{
    ZoneScoped;

    uint64_t bufsize;
    const auto mipChain = GetMipChain( mips, bitmap.Width(), bitmap.Height(), bufsize );
    const auto mipLevels = (uint32_t)mipChain.size();

    m_image = std::make_shared<VlkImage>( device, GetImageCreateInfo( format, bitmap.Width(), bitmap.Height(), mipLevels ) );
    m_imageView = std::make_unique<VlkImageView>( device, GetImageViewCreateInfo( *m_image, format, mipLevels ) );
    auto stagingBuffer = std::make_shared<VlkBuffer>( device, GetStagingBufferInfo( bufsize ), VlkBuffer::WillWrite | VlkBuffer::PreferHost );

    std::unique_ptr<Bitmap> tmp;
    auto bmpptr = &bitmap;
    auto bufptr = (uint8_t*)stagingBuffer->Ptr();
    for( uint32_t level = 0; level < mipLevels; level++ )
    {
        const MipData& mipdata = mipChain[level];
        memcpy( bufptr, bmpptr->Data(), mipdata.size );
        bufptr += mipdata.size;
        stagingBuffer->Flush( mipdata.offset, mipdata.size );

        if( level < mipLevels-1 )
        {
            ZoneScopedN( "Mip downscale" );
            ZoneTextF( "Level %u, %u x %u, %u bytes", level, mipChain[level+1].width, mipChain[level+1].height, mipChain[level+1].size );

            tmp = bmpptr->ResizeNew( mipChain[level+1].width, mipChain[level+1].height, td );
            bmpptr = tmp.get();
        }
    }

    Upload( device, mipChain, std::move( stagingBuffer ), fencesOut );
}

void Texture::Upload( VlkDevice& device, const std::vector<MipData>& mipChain, std::shared_ptr<VlkBuffer>&& stagingBuffer, std::vector<std::shared_ptr<VlkFence>>& fencesOut )
{
    const auto mipLevels = (uint32_t)mipChain.size();

    auto cmdTrn = std::make_unique<VlkCommandBuffer>( *device.GetCommandPool( QueueType::Transfer ) );
    cmdTrn->Begin( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );

    for( uint32_t level = 0; level < mipLevels; level++ )
    {
        ZoneVk( device, *cmdTrn, "Texture upload", true );
        WriteBarrier( *cmdTrn, level );
        const MipData& mipdata = mipChain[level];
        VkBufferImageCopy region = {
            .bufferOffset = mipdata.offset,
            .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, level, 0, 1 },
            .imageExtent = { mipdata.width, mipdata.height, 1 }
        };
        vkCmdCopyBufferToImage( *cmdTrn, *stagingBuffer, *m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );
    }

    const auto shareQueue = device.GetQueueInfo( QueueType::Graphic ).shareTransfer;
    const auto trnQueue = device.GetQueueInfo( QueueType::Transfer ).idx;
    const auto gfxQueue = device.GetQueueInfo( QueueType::Graphic ).idx;
    if( shareQueue )
    {
        ReadBarrier( *cmdTrn, mipLevels );
    }
    else
    {
        ReadBarrierTrn( *cmdTrn, mipLevels, trnQueue, gfxQueue );
    }
    cmdTrn->End();

    auto fenceTrn = std::make_shared<VlkFence>( device );
    device.Submit( *cmdTrn, *fenceTrn );
    device.GetGarbage()->Recycle( fenceTrn, {
        std::move( cmdTrn ),
        std::move( stagingBuffer ),
        m_image
    } );
    fencesOut.emplace_back( std::move( fenceTrn ) );

    if( !shareQueue )
    {
        auto cmdGfx = std::make_unique<VlkCommandBuffer>( *device.GetCommandPool( QueueType::Graphic ) );
        cmdGfx->Begin( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
        ReadBarrierGfx( *cmdGfx, mipLevels, trnQueue, gfxQueue );
        cmdGfx->End();

        auto fenceGfx = std::make_shared<VlkFence>( device );
        device.Submit( *cmdGfx, *fenceGfx );
        device.GetGarbage()->Recycle( fenceGfx, {
            std::move( cmdGfx ),
            m_image
        } );
        fencesOut.emplace_back( std::move( fenceGfx ) );
    }
}

static std::vector<MipData> CalcMipLevels( uint32_t width, uint32_t height, uint64_t& total )
{
    const auto mipLevels = (uint32_t)std::floor( std::log2( std::max( width, height ) ) ) + 1;
    uint64_t offset = 0;
    std::vector<MipData> levels( mipLevels );
    for( uint32_t i=0; i<mipLevels; i++ )
    {
        const uint64_t size = width * height * 4;
        levels[i] = { width, height, offset, size };
        width = std::max( 1u, width / 2 );
        height = std::max( 1u, height / 2 );
        offset += size;
    }
    total = offset;
    return levels;
}

std::vector<MipData> Texture::GetMipChain( bool mips, uint32_t width, uint32_t height, uint64_t& bufsize )
{
    std::vector<MipData> mipChain;
    if( mips )
    {
        mipChain = CalcMipLevels( width, height, bufsize );
    }
    else
    {
        bufsize = width * height * 4;
        mipChain.emplace_back( width, height, 0, bufsize );
    }
    return mipChain;
}

void Texture::WriteBarrier( VkCommandBuffer cmdbuf, uint32_t mip )
{
    VkImageMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = *m_image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, mip, 1, 0, 1 }
    };
    VkDependencyInfo deps = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier
    };
    vkCmdPipelineBarrier2( cmdbuf, &deps );
}

void Texture::ReadBarrier( VkCommandBuffer cmdbuf, uint32_t mipLevels )
{
    VkImageMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = *m_image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1 }
    };
    VkDependencyInfo deps = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier
    };
    vkCmdPipelineBarrier2( cmdbuf, &deps );
}

void Texture::ReadBarrierTrn( VkCommandBuffer cmdbuf, uint32_t mipLevels, uint32_t trnQueue, uint32_t gfxQueue )
{
    VkImageMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = trnQueue,
        .dstQueueFamilyIndex = gfxQueue,
        .image = *m_image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1 }
    };
    VkDependencyInfo deps = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier
    };
    vkCmdPipelineBarrier2( cmdbuf, &deps );
}

void Texture::ReadBarrierGfx( VkCommandBuffer cmdbuf, uint32_t mipLevels, uint32_t trnQueue, uint32_t gfxQueue )
{
    VkImageMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = trnQueue,
        .dstQueueFamilyIndex = gfxQueue,
        .image = *m_image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1 }
    };
    VkDependencyInfo deps = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier
    };
    vkCmdPipelineBarrier2( cmdbuf, &deps );
}
