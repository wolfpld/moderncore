#ifndef __VLKSWAPCHAIN_HPP__
#define __VLKSWAPCHAIN_HPP__

#include <stdint.h>
#include <vulkan/vulkan.h>

#include "VlkSwapchainProperties.hpp"

class VlkSwapchain
{
public:
    VlkSwapchain( VkPhysicalDevice physicalDevice, VkSurfaceKHR surface );
    ~VlkSwapchain();

private:
    VlkSwapchainProperties m_properties;
    VkSurfaceFormatKHR m_format;
    VkExtent2D m_extent;
    uint32_t m_imageCount;
};

#endif
