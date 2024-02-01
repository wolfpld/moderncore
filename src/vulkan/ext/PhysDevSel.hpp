#pragma once

#include <vector>
#include <vulkan/vulkan.h>

namespace PhysDevSel
{

enum Flag
{
    RequireGraphic      = 1 << 0,
    RequireCompute      = 1 << 1,
    PreferIntegrated    = 1 << 2,
};

[[nodiscard]] VkPhysicalDevice PickBest( const std::vector<VkPhysicalDevice>& list, VkSurfaceKHR presentSurface = VK_NULL_HANDLE, int flags = 0 );

}
