#include <vector>
#include <vulkan/vk_enum_string_helper.h>

#include "VlkSwapchain.hpp"
#include "../util/Logs.hpp"

VlkSwapchain::VlkSwapchain( VkPhysicalDevice physicalDevice, VkSurfaceKHR surface )
    : m_properties( physicalDevice, surface )
{
#ifdef DEBUG
    const auto caps = m_properties.GetCapabilities();
    mclog( LogLevel::Debug, "Swapchain image count: min %d, max %d", caps.minImageCount, caps.maxImageCount );
    mclog( LogLevel::Debug, "Swapchain surface formats:" );
    for( auto& format : m_properties.GetFormats() )
    {
        mclog( LogLevel::Debug, "  %s / %s", string_VkFormat( format.format ), string_VkColorSpaceKHR( format.colorSpace ) );
    }
    mclog( LogLevel::Debug, "Swapchain present modes:" );
    for( auto& mode : m_properties.GetPresentModes() )
    {
        mclog( LogLevel::Debug, "  %s", string_VkPresentModeKHR( mode ) );
    }
#endif
}

VlkSwapchain::~VlkSwapchain()
{
}
