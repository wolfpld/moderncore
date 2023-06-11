#ifndef __VLKSWAPCHAIN_HPP__
#define __VLKSWAPCHAIN_HPP__

#include <stdint.h>
#include <vulkan/vulkan.h>

#include "VlkSwapchainProperties.hpp"

class VlkDevice;

class VlkSwapchain
{
public:
    VlkSwapchain( VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, const VlkDevice& device );
    ~VlkSwapchain();

private:
    VlkSwapchainProperties m_properties;

    VkSurfaceFormatKHR m_format;
    VkPresentModeKHR m_presentMode;
    VkExtent2D m_extent;
    uint32_t m_imageCount;

    VkDevice m_device;
    VkSwapchainKHR m_swapchain;
};

#endif
