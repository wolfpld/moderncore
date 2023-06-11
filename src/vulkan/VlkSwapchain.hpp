#ifndef __VLKSWAPCHAIN_HPP__
#define __VLKSWAPCHAIN_HPP__

#include <stdint.h>
#include <vector>
#include <vulkan/vulkan.h>

#include "VlkSwapchainProperties.hpp"

class VlkDevice;

class VlkSwapchain
{
public:
    VlkSwapchain( VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, const VlkDevice& device );
    ~VlkSwapchain();

    operator VkSwapchainKHR() const { return m_swapchain; }

private:
    VlkSwapchainProperties m_properties;

    VkSurfaceFormatKHR m_format;
    VkPresentModeKHR m_presentMode;
    VkExtent2D m_extent;

    VkDevice m_device;
    VkSwapchainKHR m_swapchain;

    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;
};

#endif
