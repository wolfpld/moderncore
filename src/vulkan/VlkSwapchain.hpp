#pragma once

#include <stdint.h>
#include <vector>
#include <vulkan/vulkan.h>

#include "VlkBase.hpp"
#include "VlkSwapchainProperties.hpp"
#include "util/NoCopy.hpp"

class VlkDevice;

class VlkSwapchain : public VlkBase
{
public:
    VlkSwapchain( const VlkDevice& device, VkSurfaceKHR surface, const VkExtent2D& extent );
    ~VlkSwapchain();

    NoCopy( VlkSwapchain );

    void PresentBarrier( VkCommandBuffer cmd, uint32_t imageIndex );
    void RenderBarrier( VkCommandBuffer cmd, uint32_t imageIndex );
    void TransferBarrier( VkCommandBuffer cmd, uint32_t imageIndex );

    [[nodiscard]] VkFormat GetFormat() const { return m_format.format; }
    [[nodiscard]] const VkExtent2D& GetExtent() const { return m_extent; }
    [[nodiscard]] const std::vector<VkImage>& GetImages() const { return m_images; }
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
    std::vector<VkImageLayout> m_imageLayouts;
};
