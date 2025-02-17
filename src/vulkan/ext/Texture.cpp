#include <string.h>
#include <vulkan/vulkan.h>
#include <tracy/TracyVulkan.hpp>

#include "Texture.hpp"
#include "util/Bitmap.hpp"
#include "util/Tracy.hpp"
#include "vulkan/VlkBuffer.hpp"
#include "vulkan/VlkCommandBuffer.hpp"
#include "vulkan/VlkDevice.hpp"
#include "vulkan/VlkFence.hpp"

Texture::Texture( GarbageChute& garbage, VlkDevice& device, const Bitmap& bitmap, VkFormat format )
    : m_layout( VK_IMAGE_LAYOUT_UNDEFINED )
    , m_access( VK_ACCESS_NONE )
{
    VkImageCreateInfo imageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {
            .width = bitmap.Width(),
            .height = bitmap.Height(),
            .depth = 1
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    m_image = std::make_shared<VlkImage>( device, imageInfo );

    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = *m_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1
        }
    };
    m_imageView = std::make_unique<VlkImageView>( device, viewInfo );

    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = bitmap.Width() * bitmap.Height() * 4,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    auto stagingBuffer = std::make_unique<VlkBuffer>( device, bufferInfo, VlkBuffer::WillWrite | VlkBuffer::PreferHost );
    memcpy( stagingBuffer->Ptr(), bitmap.Data(), bitmap.Width() * bitmap.Height() * 4 );
    stagingBuffer->Flush();

    auto cmdBuf = std::make_unique<VlkCommandBuffer>( *device.GetCommandPool( QueueType::Graphic ) );
    cmdBuf->Begin( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );

#ifdef TRACY_ENABLE
    auto tracyCtx = device.GetTracyContext();
    tracy::VkCtxScope* tracyScope = nullptr;
    if( tracyCtx ) ZoneVkNew( tracyCtx, tracyScope, *cmdBuf, "Texture upload", true );
#endif

    TransitionLayout( *cmdBuf, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT );

    VkBufferImageCopy region = {
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1
        },
        .imageExtent = {
            .width = bitmap.Width(),
            .height = bitmap.Height(),
            .depth = 1
        }
    };
    vkCmdCopyBufferToImage( *cmdBuf, *stagingBuffer, *m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );

    TransitionLayout( *cmdBuf, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );

#ifdef TRACY_ENABLE
    delete tracyScope;
#endif

    cmdBuf->End();

    auto fence = std::make_shared<VlkFence>( device );
    device.Submit( *cmdBuf, *fence );
    garbage.Recycle( std::move( fence ), {
        std::move( cmdBuf ),
        std::move( stagingBuffer ),
        m_image
    } );
}

void Texture::TransitionLayout( VkCommandBuffer cmdBuf, VkImageLayout layout, VkAccessFlagBits access, VkPipelineStageFlagBits src, VkPipelineStageFlagBits dst )
{
    if( m_layout == layout && m_access == access ) return;

    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = m_access,
        .dstAccessMask = access,
        .oldLayout = m_layout,
        .newLayout = layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = *m_image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1
        },
    };
    vkCmdPipelineBarrier( cmdBuf, src, dst, 0, 0, nullptr, 0, nullptr, 1, &barrier );

    m_layout = layout;
    m_access = access;
}
