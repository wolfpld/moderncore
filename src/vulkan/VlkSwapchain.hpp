#pragma once

#include <stdint.h>
#include <vector>
#include <vulkan/vulkan.h>

#include "VlkSwapchainProperties.hpp"
#include "../util/NoCopy.hpp"

class VlkDevice;

class VlkSwapchain
{
public:
    VlkSwapchain( VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, const VlkDevice& device );
    ~VlkSwapchain();

    NoCopy( VlkSwapchain );

    [[nodiscard]] VkFormat GetFormat() const { return m_format.format; }
    [[nodiscard]] uint32_t GetWidth() const { return m_extent.width; }
    [[nodiscard]] uint32_t GetHeight() const { return m_extent.height; }
    [[nodiscard]] const std::vector<VkImageView>& GetImageViews() const { return m_imageViews; }

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
