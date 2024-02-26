#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

class VlkPhysicalDevice;

namespace PhysDevSel
{

enum Flag
{
    RequireGraphic      = 1 << 0,
    RequireCompute      = 1 << 1,
    PreferIntegrated    = 1 << 2,
};

[[nodiscard]] std::shared_ptr<VlkPhysicalDevice> PickBest( const std::vector<std::shared_ptr<VlkPhysicalDevice>>& list, VkSurfaceKHR presentSurface = VK_NULL_HANDLE, int flags = 0 );

}
