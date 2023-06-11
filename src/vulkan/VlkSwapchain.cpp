#include <algorithm>
#include <assert.h>
#include <stdint.h>
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

    mclog( LogLevel::Info, "Swapchain format: %s / %s", string_VkFormat( m_format.format ), string_VkColorSpaceKHR( m_format.colorSpace ) );

    const auto caps = m_properties.GetCapabilities();
    m_extent.width = caps.currentExtent.width == UINT32_MAX ?
        std::clamp( 1650u, caps.minImageExtent.width, caps.maxImageExtent.width ) : caps.currentExtent.width;
    m_extent.height = caps.currentExtent.height == UINT32_MAX ?
        std::clamp( 1050u, caps.minImageExtent.height, caps.maxImageExtent.height ) : caps.currentExtent.height;

    mclog( LogLevel::Info, "Swapchain extent: %ux%u", m_extent.width, m_extent.height );

    m_imageCount = std::max( 2u, caps.minImageCount );
    if( caps.maxImageCount > 0 && m_imageCount > caps.maxImageCount ) m_imageCount = caps.maxImageCount;

    mclog( LogLevel::Info, "Swapchain image count: %u", m_imageCount );
}

VlkSwapchain::~VlkSwapchain()
{
}
