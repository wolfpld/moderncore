#include "ImageSync.hpp"

void ImageSync::Barrier( VkCommandBuffer cmdBuf, VkImage image,
    VkPipelineStageFlags2 srcStages, VkAccessFlags2 srcAccess,
    VkPipelineStageFlags2 dstStages, VkAccessFlags2 dstAccess,
    VkImageLayout oldLayout, VkImageLayout newLayout,
    uint32_t srcFamily, uint32_t dstFamily,
    uint32_t baseLevel, uint32_t levelCount )
{
    const VkImageMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = srcStages,
        .srcAccessMask = srcAccess,
        .dstStageMask = dstStages,
        .dstAccessMask = dstAccess,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = srcFamily,
        .dstQueueFamilyIndex = dstFamily,
        .image = image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, baseLevel, levelCount, 0, 1 }
    };
    const VkDependencyInfo deps = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier
    };
    vkCmdPipelineBarrier2( cmdBuf, &deps );
}
