#include <vector>

#include "VlkSwapchainProperties.hpp"

VlkSwapchainProperties::VlkSwapchainProperties( VkPhysicalDevice physicalDevice, VkSurfaceKHR surface )
{
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR( physicalDevice, surface, &m_capabilities );

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR( physicalDevice, surface, &formatCount, nullptr );
    m_formats.resize( formatCount );
    vkGetPhysicalDeviceSurfaceFormatsKHR( physicalDevice, surface, &formatCount, m_formats.data() );

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR( physicalDevice, surface, &presentModeCount, nullptr );
    m_presentModes.resize( presentModeCount );
    vkGetPhysicalDeviceSurfacePresentModesKHR( physicalDevice, surface, &presentModeCount, m_presentModes.data() );
}
