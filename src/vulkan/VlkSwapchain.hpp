#ifndef __VLKSWAPCHAIN_HPP__
#define __VLKSWAPCHAIN_HPP__

#include <vulkan/vulkan.h>

#include "VlkSwapchainProperties.hpp"

class VlkSwapchain
{
public:
    VlkSwapchain( VkPhysicalDevice physicalDevice, VkSurfaceKHR surface );
    ~VlkSwapchain();

private:
    VlkSwapchainProperties m_properties;
};

#endif
