#include <assert.h>
#include <vector>
#include <vulkan/vk_enum_string_helper.h>

#include "VlkInfo.hpp"
#include "VlkSwapchain.hpp"
#include "VlkSwapchainFormats.hpp"
#include "../util/Logs.hpp"

VlkSwapchain::VlkSwapchain( VkPhysicalDevice physicalDevice, VkSurfaceKHR surface )
    : m_properties( physicalDevice, surface )
    , m_format { VK_FORMAT_UNDEFINED }
{
#ifdef DEBUG
    PrintSwapchainProperties( m_properties );
#endif

    for( auto& format: m_properties.GetFormats() )
    {
        for( auto& valid : SupportedSwapchainFormats )
        {
            if( format.format == valid.format && format.colorSpace == valid.colorSpace )
            {
                m_format = format;
                break;
            }
        }
        if( m_format.format != VK_FORMAT_UNDEFINED ) break;
    }
    assert( m_format.format != VK_FORMAT_UNDEFINED );

    mclog( LogLevel::Info, "Using swapchain format: %s / %s", string_VkFormat( m_format.format ), string_VkColorSpaceKHR( m_format.colorSpace ) );
}

VlkSwapchain::~VlkSwapchain()
{
}
