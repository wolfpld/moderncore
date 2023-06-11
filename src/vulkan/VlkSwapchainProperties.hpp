#ifndef __VLKSWAPCHAINPROPERTIES_HPP__
#define __VLKSWAPCHAINPROPERTIES_HPP__

#include <vector>
#include <vulkan/vulkan.h>

class VlkSwapchainProperties
{
public:
    VlkSwapchainProperties( VkPhysicalDevice physicalDevice, VkSurfaceKHR surface );

    [[nodiscard]] VkSurfaceCapabilitiesKHR GetCapabilities() const { return m_capabilities; }
    [[nodiscard]] const std::vector<VkSurfaceFormatKHR>& GetFormats() const { return m_formats; }
    [[nodiscard]] const std::vector<VkPresentModeKHR>& GetPresentModes() const { return m_presentModes; }

private:
    VkSurfaceCapabilitiesKHR m_capabilities;
    std::vector<VkSurfaceFormatKHR> m_formats;
    std::vector<VkPresentModeKHR> m_presentModes;
};

#endif
