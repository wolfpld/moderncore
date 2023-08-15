#include <inttypes.h>
#include <vector>
#include <vulkan/vk_enum_string_helper.h>

#include "VlkDevice.hpp"
#include "VlkInfo.hpp"
#include "VlkPhysicalDevice.hpp"
#include "VlkSwapchainProperties.hpp"
#include "../util/Logs.hpp"

void PrintSwapchainProperties( const VlkSwapchainProperties &properties )
{
    const auto caps = properties.GetCapabilities();
    mclog( LogLevel::Debug, "Swapchain image count: min %d, max %d", caps.minImageCount, caps.maxImageCount );
    mclog( LogLevel::Debug, "Swapchain surface formats:" );
    for( auto& format : properties.GetFormats() )
    {
        mclog( LogLevel::Debug, "  %s / %s", string_VkFormat( format.format ), string_VkColorSpaceKHR( format.colorSpace ) );
    }
    mclog( LogLevel::Debug, "Swapchain present modes:" );
    for( auto& mode : properties.GetPresentModes() )
    {
        mclog( LogLevel::Debug, "  %s", string_VkPresentModeKHR( mode ) );
    }
}
