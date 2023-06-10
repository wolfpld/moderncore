#include <vector>
#include <vulkan/vk_enum_string_helper.h>

#include "VlkSwapchain.hpp"
#include "../util/Logs.hpp"

VlkSwapchain::VlkSwapchain( VkPhysicalDevice physicalDevice, VkSurfaceKHR surface )
{
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR( physicalDevice, surface, &caps );

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR( physicalDevice, surface, &formatCount, nullptr );
    std::vector<VkSurfaceFormatKHR> formats( formatCount );
    vkGetPhysicalDeviceSurfaceFormatsKHR( physicalDevice, surface, &formatCount, formats.data() );

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR( physicalDevice, surface, &presentModeCount, nullptr );
    std::vector<VkPresentModeKHR> presentModes( presentModeCount );
    vkGetPhysicalDeviceSurfacePresentModesKHR( physicalDevice, surface, &presentModeCount, presentModes.data() );

#ifdef DEBUG
    mclog( LogLevel::Debug, "Swapchain image count: min %d, max %d", caps.minImageCount, caps.maxImageCount );
    mclog( LogLevel::Debug, "Swapchain surface formats:" );
    for( auto& format : formats )
    {
        mclog( LogLevel::Debug, "  %s / %s", string_VkFormat( format.format ), string_VkColorSpaceKHR( format.colorSpace ) );
    }
    mclog( LogLevel::Debug, "Swapchain present modes:" );
    for( auto& mode : presentModes )
    {
        mclog( LogLevel::Debug, "  %s", string_VkPresentModeKHR( mode ) );
    }
#endif
}

VlkSwapchain::~VlkSwapchain()
{
}
