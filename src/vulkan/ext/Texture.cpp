#include <string.h>
#include <vulkan/vulkan.h>
#include <tracy/TracyVulkan.hpp>

#include "Texture.hpp"
#include "util/Bitmap.hpp"
#include "vulkan/VlkBuffer.hpp"
#include "vulkan/VlkCommandBuffer.hpp"
#include "vulkan/VlkDevice.hpp"
#include "vulkan/VlkFence.hpp"
#include "vulkan/VlkGarbage.hpp"
#include "vulkan/ext/Tracy.hpp"

Texture::Texture( VlkDevice& device, const Bitmap& bitmap, VkFormat format, std::vector<std::shared_ptr<VlkFence>>& fencesOut )
{
    VkImageCreateInfo imageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = { bitmap.Width(), bitmap.Height(), 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    m_image = std::make_shared<VlkImage>( device, imageInfo );

    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = *m_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    m_imageView = std::make_unique<VlkImageView>( device, viewInfo );

    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = bitmap.Width() * bitmap.Height() * 4,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    auto stagingBuffer = std::make_shared<VlkBuffer>( device, bufferInfo, VlkBuffer::WillWrite | VlkBuffer::PreferHost );
    memcpy( stagingBuffer->Ptr(), bitmap.Data(), bitmap.Width() * bitmap.Height() * 4 );
    stagingBuffer->Flush();

    if( device.GetQueueInfo( QueueType::Graphic ).shareTransfer )
    {
        auto cmdbuf = std::make_unique<VlkCommandBuffer>( *device.GetCommandPool( QueueType::Graphic ) );
        cmdbuf->Begin( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
        {
            ZoneVk( device, *cmdbuf, "Texture upload", true );
            WriteBarrier( *cmdbuf );
            VkBufferImageCopy region = {
                .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                .imageExtent = { bitmap.Width(), bitmap.Height(), 1 }
            };
            vkCmdCopyBufferToImage( *cmdbuf, *stagingBuffer, *m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );
            ReadBarrier( *cmdbuf );
        }
        cmdbuf->End();

        auto fence = std::make_shared<VlkFence>( device );
        device.Submit( *cmdbuf, *fence );
        device.GetGarbage()->Recycle( fence, {
            std::move( cmdbuf ),
            std::move( stagingBuffer ),
            m_image
        } );
        fencesOut.emplace_back( std::move( fence ) );
    }
    else
    {
        const auto trnQueue = device.GetQueueInfo( QueueType::Transfer ).idx;
        const auto gfxQueue = device.GetQueueInfo( QueueType::Graphic ).idx;

        auto cmdTrn = std::make_unique<VlkCommandBuffer>( *device.GetCommandPool( QueueType::Transfer ) );
        cmdTrn->Begin( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
        {
            ZoneVk( device, *cmdTrn, "Texture upload", true );
            WriteBarrier( *cmdTrn );
            VkBufferImageCopy region = {
                .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                .imageExtent = { bitmap.Width(), bitmap.Height(), 1 }
            };
            vkCmdCopyBufferToImage( *cmdTrn, *stagingBuffer, *m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );
            ReadBarrierTrn( *cmdTrn, trnQueue, gfxQueue );
        }
        cmdTrn->End();

        auto fenceTrn = std::make_shared<VlkFence>( device );
        device.Submit( *cmdTrn, *fenceTrn );
        device.GetGarbage()->Recycle( fenceTrn, {
            std::move( cmdTrn ),
            stagingBuffer,
            m_image
        } );
        fencesOut.emplace_back( std::move( fenceTrn ) );

        auto cmdGfx = std::make_unique<VlkCommandBuffer>( *device.GetCommandPool( QueueType::Graphic ) );
        cmdGfx->Begin( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
        ReadBarrierGfx( *cmdGfx, trnQueue, gfxQueue );
        cmdGfx->End();

        auto fenceGfx = std::make_shared<VlkFence>( device );
        device.Submit( *cmdGfx, *fenceGfx );
        device.GetGarbage()->Recycle( fenceGfx, {
            std::move( cmdGfx ),
            std::move( stagingBuffer ),
            m_image
        } );
        fencesOut.emplace_back( std::move( fenceGfx ) );
    }
}

void Texture::WriteBarrier( VkCommandBuffer cmdbuf )
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
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    VkDependencyInfo deps = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier
    };
    vkCmdPipelineBarrier2( cmdbuf, &deps );
}

void Texture::ReadBarrier( VkCommandBuffer cmdbuf )
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
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    VkDependencyInfo deps = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier
    };
    vkCmdPipelineBarrier2( cmdbuf, &deps );
}

void Texture::ReadBarrierTrn( VkCommandBuffer cmdbuf, uint32_t trnQueue, uint32_t gfxQueue )
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
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    VkDependencyInfo deps = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier
    };
    vkCmdPipelineBarrier2( cmdbuf, &deps );
}

void Texture::ReadBarrierGfx( VkCommandBuffer cmdbuf, uint32_t trnQueue, uint32_t gfxQueue )
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
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    VkDependencyInfo deps = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier
    };
    vkCmdPipelineBarrier2( cmdbuf, &deps );
}
