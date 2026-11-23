#include <algorithm>
#include <cmath>
#include <string.h>
#include <vulkan/vulkan.h>
#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>

#include "Texture.hpp"
#include "ImageSync.hpp"
#include "MipGen.hpp"
#include "vulkan/VlkSemaphore.hpp"
#include "vulkan/VlkStagingRing.hpp"
#include "util/Bitmap.hpp"
#include "util/BitmapHdr.hpp"
#include "util/BitmapHdrHalf.hpp"
#include "util/Logs.hpp"
#include "util/Panic.hpp"
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
    uint64_t size;
};

static std::vector<MipData> CalcMipLevels( uint32_t width, uint32_t height, uint32_t bpp )
{
    const auto mipLevels = (uint32_t)std::floor( std::log2( std::max( width, height ) ) ) + 1;
    std::vector<MipData> levels( mipLevels );
    for( uint32_t i=0; i<mipLevels; i++ )
    {
        levels[i] = {
            .width = width,
            .height = height,
            .size = uint64_t( width ) * height * bpp
        };
        width = std::max( 1u, width / 2 );
        height = std::max( 1u, height / 2 );
    }
    return levels;
}

static std::vector<MipData> GetMipChain( bool mips, uint32_t width, uint32_t height, uint32_t bpp )
{
    std::vector<MipData> mipChain;
    if( mips )
    {
        mipChain = CalcMipLevels( width, height, bpp );
    }
    else
    {
        mipChain.push_back( {
            .width = width,
            .height = height,
            .size = uint64_t( width ) * height * bpp
        } );
    }
    return mipChain;
}

static inline VkImageCreateInfo GetImageCreateInfo( VkFormat format, uint32_t width, uint32_t height, uint32_t mipLevels, bool hostImageCopy, bool mipStorage )
{
    return {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = mipStorage ? VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT : VkImageCreateFlags( 0 ),
        .imageType = VK_IMAGE_TYPE_2D,
        .format = mipStorage ? MipGen::StorageViewFormat( format ) : format,
        .extent = { width, height, 1 },
        .mipLevels = mipLevels,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VkImageUsageFlags( VK_IMAGE_USAGE_SAMPLED_BIT |
            ( hostImageCopy ?
                VK_IMAGE_USAGE_HOST_TRANSFER_BIT :
                ( VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                    ( mipStorage ? VK_IMAGE_USAGE_STORAGE_BIT : 0u ) )
            ) ),
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
}

static inline VkImageViewCreateInfo GetImageViewCreateInfo( VkImage image, VkFormat format, uint32_t mipLevels, bool restrictStorage )
{
    static constexpr VkImageViewUsageCreateInfo usage = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
    };
    return {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = restrictStorage ? &usage : nullptr,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1 }
    };
}

static inline VkBufferCreateInfo GetStagingBufferInfo( uint64_t size, VkBufferUsageFlags usage )
{
    return {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
}

template<typename T>
static void HostCopy( VlkDevice& device, VlkImage& image, const std::vector<MipData>& mipChain, std::unique_ptr<T>&& tmp, const T* bmpptr, TaskDispatch* td )
{
    const auto mipLevels = (uint32_t)mipChain.size();
    for( uint32_t level = 0; level < mipLevels; level++ )
    {
        const MipData& mipdata = mipChain[level];

        VkHostImageLayoutTransitionInfo transition = {
            .sType = VK_STRUCTURE_TYPE_HOST_IMAGE_LAYOUT_TRANSITION_INFO,
            .image = image,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, level, 1, 0, 1 }
        };
        vkTransitionImageLayout( device, 1, &transition );

        VkMemoryToImageCopy region = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_TO_IMAGE_COPY,
            .pHostPointer = bmpptr->Data(),
            .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, level, 0, 1 },
            .imageExtent = { mipdata.width, mipdata.height, 1 },
        };
        VkCopyMemoryToImageInfo copy = {
            .sType = VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INFO,
            .dstImage = image,
            .dstImageLayout = VK_IMAGE_LAYOUT_GENERAL,
            .regionCount = 1,
            .pRegions = &region
        };
        vkCopyMemoryToImage( device, &copy );

        transition.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        transition.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkTransitionImageLayout( device, 1, &transition );

        if( level < mipLevels-1 )
        {
            ZoneScopedN( "Mip downscale" );
            ZoneTextF( "Level %u, %u x %u, %llu bytes", level, mipChain[level+1].width, mipChain[level+1].height, (unsigned long long)mipChain[level+1].size );

            tmp = bmpptr->ResizeNew( mipChain[level+1].width, mipChain[level+1].height, td );
            bmpptr = tmp.get();
        }
    }
}

static void ReadbackBuffer( VlkDevice& device, uint32_t width, uint32_t height, VkBuffer buffer, VkImage image )
{
    auto cmd = std::make_unique<VlkCommandBuffer>( *device.GetCommandPool( QueueType::Graphic ) );
    cmd->Begin( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );

    // readback barrier
    {
        const VkImageMemoryBarrier2 barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };
        const VkDependencyInfo deps = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &barrier
        };
        vkCmdPipelineBarrier2( *cmd, &deps );
    }

    const VkBufferImageCopy region = {
        .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .imageExtent = { width, height, 1 }
    };
    vkCmdCopyImageToBuffer( *cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region );

    // read barrier
    {
        const VkImageMemoryBarrier2 barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };
        const VkDependencyInfo deps = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &barrier
        };
        vkCmdPipelineBarrier2( *cmd, &deps );
    }
    cmd->End();

    auto fence = std::make_shared<VlkFence>( device );
    device.Submit( *cmd, *fence );
    fence->Wait();
}

template<typename T>
static void ReadbackHost( VlkDevice& device, std::shared_ptr<T>& bitmap, VkImage image )
{
    VkHostImageLayoutTransitionInfo transition = {
        .sType = VK_STRUCTURE_TYPE_HOST_IMAGE_LAYOUT_TRANSITION_INFO,
        .image = image,
        .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    vkTransitionImageLayout( device, 1, &transition );

    VkImageToMemoryCopy region = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_TO_MEMORY_COPY,
        .pHostPointer = bitmap->Data(),
        .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .imageExtent = { bitmap->Width(), bitmap->Height(), 1 }
    };
    VkCopyImageToMemoryInfo copy = {
        .sType = VK_STRUCTURE_TYPE_COPY_IMAGE_TO_MEMORY_INFO,
        .srcImage = image,
        .srcImageLayout = VK_IMAGE_LAYOUT_GENERAL,
        .regionCount = 1,
        .pRegions = &region
    };
    vkCopyImageToMemory( device, &copy );

    transition.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    transition.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkTransitionImageLayout( device, 1, &transition );
}

template<typename T>
void Texture::Upload( VlkDevice& device, const std::vector<MipData>& mipChain, uint32_t bpp,
                      const T* bmpptr, std::unique_ptr<T>&& tmp, TaskDispatch* td,
                      std::vector<std::shared_ptr<VlkFence>>& fencesOut, MipGen* mipGen )
{
    const auto mipLevels = (uint32_t)mipChain.size();
    auto& ring = device.GetStagingRing();

    if( mipGen )
    {
        const MipData& mipdata = mipChain[0];
        ring.Level( *m_image, 0, mipdata.width, mipdata.height, bpp, bmpptr->Data(),
            [this]( VkCommandBuffer cmdbuf ) { WriteBarrier( cmdbuf, 0 ); } );

        FinishUploadGpu( device, *mipGen, mipLevels, fencesOut );
        return;
    }

    for( uint32_t level = 0; level < mipLevels; level++ )
    {
        const MipData& mipdata = mipChain[level];
        ring.Level( *m_image, level, mipdata.width, mipdata.height, bpp, bmpptr->Data(),
            [this, level]( VkCommandBuffer cmdbuf ) { WriteBarrier( cmdbuf, level ); } );

        if( level < mipLevels-1 )
        {
            ZoneScopedN( "Mip downscale" );
            ZoneTextF( "Level %u, %u x %u, %llu bytes", level+1, mipChain[level+1].width, mipChain[level+1].height, (unsigned long long)mipChain[level+1].size );

            tmp = bmpptr->ResizeNew( mipChain[level+1].width, mipChain[level+1].height, td );
            bmpptr = tmp.get();
        }
    }

    FinishUpload( device, mipLevels, fencesOut );
}

Texture::Texture( VlkDevice& device, const Bitmap& bitmap, VkFormat format, bool mips, std::vector<std::shared_ptr<VlkFence>>& fencesOut, TaskDispatch* td, MipGen* mipGen )
    : m_format( format )
    , m_width( bitmap.Width() )
    , m_height( bitmap.Height() )
{
    ZoneScoped;

    const auto mipChain = GetMipChain( mips, bitmap.Width(), bitmap.Height(), 4 );
    const auto mipLevels = (uint32_t)mipChain.size();
    const auto hostImageCopy = device.UseHostImageCopy();
    mipGen = ( mipGen && mipLevels > 1 && !hostImageCopy &&
        mipGen->Supports( format, bitmap.Width(), bitmap.Height() ) ) ? mipGen : nullptr;

    m_image = std::make_shared<VlkImage>( device, GetImageCreateInfo( format, bitmap.Width(), bitmap.Height(), mipLevels, hostImageCopy, mipGen != nullptr ) );
    m_imageView = std::make_unique<VlkImageView>( device, GetImageViewCreateInfo( *m_image, format, mipLevels, mipGen != nullptr ) );

    if( hostImageCopy )
    {
        HostCopy( device, *m_image, mipChain, std::unique_ptr<Bitmap>(), &bitmap, td );
    }
    else
    {
        Upload( device, mipChain, 4, &bitmap, std::unique_ptr<Bitmap>(), td, fencesOut, mipGen );
    }
}

Texture::Texture( VlkDevice& device, const BitmapHdr& bitmap, VkFormat format, bool mips, std::vector<std::shared_ptr<VlkFence>>& fencesOut, TaskDispatch* td, MipGen* mipGen )
    : m_format( format )
    , m_width( bitmap.Width() )
    , m_height( bitmap.Height() )
{
    ZoneScoped;

    const bool half = format == VK_FORMAT_R16G16B16A16_SFLOAT;
    const auto mipChain = GetMipChain( mips, bitmap.Width(), bitmap.Height(), half ? 8 : 16 );
    const auto mipLevels = (uint32_t)mipChain.size();
    const auto hostImageCopy = device.UseHostImageCopy();
    mipGen = ( mipGen && mipLevels > 1 && !hostImageCopy &&
        mipGen->Supports( format, bitmap.Width(), bitmap.Height() ) ) ? mipGen : nullptr;

    m_image = std::make_shared<VlkImage>( device, GetImageCreateInfo( format, bitmap.Width(), bitmap.Height(), mipLevels, hostImageCopy, mipGen != nullptr ) );
    m_imageView = std::make_unique<VlkImageView>( device, GetImageViewCreateInfo( *m_image, format, mipLevels, mipGen != nullptr ) );

    if( hostImageCopy )
    {
        if( half )
        {
            auto tmp = std::make_unique<BitmapHdrHalf>( bitmap );
            auto bmpptr = tmp.get();
            HostCopy( device, *m_image, mipChain, std::move( tmp ), bmpptr, td );
        }
        else
        {
            HostCopy( device, *m_image, mipChain, std::unique_ptr<BitmapHdr>(), &bitmap, td );
        }
    }
    else
    {
        if( half )
        {
            auto tmp = std::make_unique<BitmapHdrHalf>( bitmap );
            auto bmpptr = tmp.get();
            Upload( device, mipChain, 8, bmpptr, std::move( tmp ), td, fencesOut, mipGen );
        }
        else
        {
            Upload( device, mipChain, 16, &bitmap, std::unique_ptr<BitmapHdr>(), td, fencesOut, mipGen );
        }
    }
}

std::shared_ptr<Bitmap> Texture::ReadbackSdr( VlkDevice& device ) const
{
    ZoneScoped;
    CheckPanic( m_format == VK_FORMAT_R8G8B8A8_SRGB, "Texture format must be VK_FORMAT_R8G8B8A8_SRGB." );

    const auto bufSize = m_width * m_height * 4;
    const auto hostImageCopy = device.UseHostImageCopy();

    auto ret = std::make_shared<Bitmap>( m_width, m_height );
    if( hostImageCopy )
    {
        ReadbackHost( device, ret, *m_image );
    }
    else
    {
        auto staging = std::make_shared<VlkBuffer>( device, GetStagingBufferInfo( bufSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT ), VlkBuffer::WillRead | VlkBuffer::PreferHost );
        ReadbackBuffer( device, m_width, m_height, *staging, *m_image );
        memcpy( ret->Data(), staging->Ptr(), bufSize );
    }
    return ret;
}

std::shared_ptr<BitmapHdrHalf> Texture::ReadbackHdr( VlkDevice& device ) const
{
    ZoneScoped;
    CheckPanic( m_format == VK_FORMAT_R16G16B16A16_SFLOAT, "Texture format must be VK_FORMAT_R16G16B16A16_SFLOAT." );

    const auto bufSize = m_width * m_height * 8;
    const auto hostImageCopy = device.UseHostImageCopy();

    auto ret = std::make_shared<BitmapHdrHalf>( m_width, m_height, Colorspace::BT2020 );
    if( hostImageCopy )
    {
        ReadbackHost( device, ret, *m_image );
    }
    else
    {
        auto staging = std::make_shared<VlkBuffer>( device, GetStagingBufferInfo( bufSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT ), VlkBuffer::WillRead | VlkBuffer::PreferHost );
        ReadbackBuffer( device, m_width, m_height, *staging, *m_image );
        memcpy( ret->Data(), staging->Ptr(), bufSize );
    }
    return ret;
}

void Texture::FinishUpload( VlkDevice& device, uint32_t mipLevels, std::vector<std::shared_ptr<VlkFence>>& fencesOut )
{
    const auto shareQueue = device.GetQueueInfo( QueueType::Graphic ).shareTransfer;
    const auto txQueue = device.GetQueueInfo( QueueType::Transfer ).idx;
    const auto gfxQueue = device.GetQueueInfo( QueueType::Graphic ).idx;
    auto cmdTx = std::make_unique<VlkCommandBuffer>( *device.GetCommandPool( QueueType::Transfer ) );
    cmdTx->Begin( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
    {
        ZoneVk( device, *cmdTx, "Texture upload finalize", true );

        if( shareQueue )
        {
            ReadBarrier( *cmdTx, mipLevels );
        }
        else
        {
            ReadBarrierTx( *cmdTx, mipLevels, txQueue, gfxQueue );
        }
    }
    cmdTx->End();

    auto fenceTrn = std::make_shared<VlkFence>( device );
    device.Submit( *cmdTx, *fenceTrn );
    device.GetGarbage()->Recycle( fenceTrn, {
        std::move( cmdTx ),
        m_image
    } );
    fencesOut.emplace_back( std::move( fenceTrn ) );

    if( !shareQueue )
    {
        auto cmdGfx = std::make_unique<VlkCommandBuffer>( *device.GetCommandPool( QueueType::Graphic ) );
        cmdGfx->Begin( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
        ReadBarrierGfx( *cmdGfx, mipLevels, txQueue, gfxQueue );
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

void Texture::FinishUploadGpu( VlkDevice& device, MipGen& mipGen, uint32_t mipLevels, std::vector<std::shared_ptr<VlkFence>>& fencesOut )
{
    const auto txQueue = device.GetQueueInfo( QueueType::Transfer ).idx;
    const auto gfxQueue = device.GetQueueInfo( QueueType::Graphic ).idx;
    const auto compQueue = device.GetQueueInfo( QueueType::Compute ).idx;
    const bool txToComp = txQueue != compQueue;
    const bool compToGfx = compQueue != gfxQueue;
    mclog( LogLevel::Debug, "GPU mip chain: %u levels, queues tx %i comp %i gfx %i",
        mipLevels, txQueue, compQueue, gfxQueue );

    auto cmdTx = std::make_unique<VlkCommandBuffer>( *device.GetCommandPool( QueueType::Transfer ) );
    cmdTx->Begin( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
    {
        ZoneVk( device, *cmdTx, "Texture mipgen start", true );
        ImageSync::Barrier( *cmdTx, *m_image,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            txToComp ? 0 : VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            txToComp ? 0 : VK_ACCESS_2_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            txToComp ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
            txToComp ? uint32_t( txQueue ) : VK_QUEUE_FAMILY_IGNORED,
            txToComp ? uint32_t( compQueue ) : VK_QUEUE_FAMILY_IGNORED, 0, 1 );
    }
    cmdTx->End();

    auto fenceTrn = std::make_shared<VlkFence>( device );
    std::shared_ptr<VlkSemaphore> semTx;
    if( txToComp ) semTx = std::make_shared<VlkSemaphore>( device );
    device.Submit( *cmdTx, QueueType::Transfer, *fenceTrn,
        VK_NULL_HANDLE, VK_PIPELINE_STAGE_2_NONE,
        semTx ? *semTx : VK_NULL_HANDLE, VK_PIPELINE_STAGE_2_TRANSFER_BIT );
    std::shared_ptr<VlkBase> txObj = std::move( cmdTx );
    device.GetGarbage()->Recycle( fenceTrn, std::move( txObj ) );
    fencesOut.emplace_back( std::move( fenceTrn ) );

    auto cmdMip = std::make_unique<VlkCommandBuffer>( *device.GetCommandPool( QueueType::Compute ) );
    cmdMip->Begin( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
    std::vector<std::shared_ptr<VlkBase>> chainViews;
    {
        ZoneVk( device, *cmdMip, "Texture mipgen", true );
        mipGen.GenerateChain( *cmdMip, *m_image, m_format, m_width, m_height, mipLevels,
            txToComp ? uint32_t( txQueue ) : VK_QUEUE_FAMILY_IGNORED,
            compToGfx ? uint32_t( gfxQueue ) : VK_QUEUE_FAMILY_IGNORED,
            chainViews );
    }
    cmdMip->End();

    auto fenceMip = std::make_shared<VlkFence>( device );
    std::shared_ptr<VlkSemaphore> semMip;
    if( compToGfx ) semMip = std::make_shared<VlkSemaphore>( device );
    device.Submit( *cmdMip, QueueType::Compute, *fenceMip,
        txToComp ? *semTx : VK_NULL_HANDLE, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        semMip ? *semMip : VK_NULL_HANDLE, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT );

    if( compToGfx )
    {
        auto cmdGfx = std::make_unique<VlkCommandBuffer>( *device.GetCommandPool( QueueType::Graphic ) );
        cmdGfx->Begin( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
        ImageSync::Barrier( *cmdGfx, *m_image,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
            uint32_t( compQueue ), uint32_t( gfxQueue ), 0, mipLevels );
        cmdGfx->End();

        auto fenceGfx = std::make_shared<VlkFence>( device );
        device.Submit( *cmdGfx, QueueType::Graphic, *fenceGfx,
            *semMip, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_NULL_HANDLE, VK_PIPELINE_STAGE_2_NONE );

        std::vector<std::shared_ptr<VlkBase>> mipRecycle { std::move( cmdMip ), std::move( semTx ) };
        std::vector<std::shared_ptr<VlkBase>> gfxRecycle { std::move( cmdGfx ), std::move( semMip ) };
        gfxRecycle.insert( gfxRecycle.end(), std::make_move_iterator( chainViews.begin() ), std::make_move_iterator( chainViews.end() ) );
        gfxRecycle.emplace_back( m_image );
        device.GetGarbage()->Recycle( fenceMip, std::move( mipRecycle ) );
        device.GetGarbage()->Recycle( fenceGfx, std::move( gfxRecycle ) );
        fencesOut.emplace_back( std::move( fenceGfx ) );
        fencesOut.emplace_back( std::move( fenceMip ) );
    }
    else
    {
        std::vector<std::shared_ptr<VlkBase>> recycle { std::move( cmdMip ), std::move( semTx ) };
        recycle.insert( recycle.end(), std::make_move_iterator( chainViews.begin() ), std::make_move_iterator( chainViews.end() ) );
        recycle.emplace_back( m_image );
        device.GetGarbage()->Recycle( fenceMip, std::move( recycle ) );
        fencesOut.emplace_back( std::move( fenceMip ) );
    }
}

void Texture::WriteBarrier( VkCommandBuffer cmdbuf, uint32_t mip )
{
    ImageSync::Barrier( cmdbuf, *m_image,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, mip, 1 );
}

void Texture::ReadBarrier( VkCommandBuffer cmdbuf, uint32_t mipLevels )
{
    ImageSync::Barrier( cmdbuf, *m_image,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, 0, mipLevels );
}

void Texture::ReadBarrierTx( VkCommandBuffer cmdbuf, uint32_t mipLevels, uint32_t trnQueue, uint32_t gfxQueue )
{
    ImageSync::Barrier( cmdbuf, *m_image,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        0, 0,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
        trnQueue, gfxQueue, 0, mipLevels );
}

void Texture::ReadBarrierGfx( VkCommandBuffer cmdbuf, uint32_t mipLevels, uint32_t trnQueue, uint32_t gfxQueue )
{
    ImageSync::Barrier( cmdbuf, *m_image,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
        trnQueue, gfxQueue, 0, mipLevels );
}
