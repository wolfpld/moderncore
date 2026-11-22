#pragma once

#include <vulkan/vulkan.h>

namespace ImageSync
{

void Barrier( VkCommandBuffer cmdBuf, VkImage image,
    VkPipelineStageFlags2 srcStages, VkAccessFlags2 srcAccess,
    VkPipelineStageFlags2 dstStages, VkAccessFlags2 dstAccess,
    VkImageLayout oldLayout, VkImageLayout newLayout,
    uint32_t srcFamily, uint32_t dstFamily,
    uint32_t baseLevel = 0, uint32_t levelCount = VK_REMAINING_MIP_LEVELS );

}
