#ifndef __VLKSWAPCHAIN_HPP__
#define __VLKSWAPCHAIN_HPP__

#include <vulkan/vulkan.h>

class VlkSwapchain
{
public:
    VlkSwapchain( VkPhysicalDevice physicalDevice, VkSurfaceKHR surface );
    ~VlkSwapchain();

private:

};

#endif
