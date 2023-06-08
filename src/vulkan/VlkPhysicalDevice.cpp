#include <algorithm>

#include "VlkPhysicalDevice.hpp"

VlkPhysicalDevice::VlkPhysicalDevice( VkPhysicalDevice physDev )
{
    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties( physDev, &count, nullptr );
    m_queueFamilyProperties.resize( count );
    vkGetPhysicalDeviceQueueFamilyProperties( physDev, &count, m_queueFamilyProperties.data() );

    vkEnumerateDeviceExtensionProperties( physDev, nullptr, &count, nullptr );
    m_extensionProperties.resize( count );
    vkEnumerateDeviceExtensionProperties( physDev, nullptr, &count, m_extensionProperties.data() );
}

bool VlkPhysicalDevice::IsGraphicCapable()
{
    return std::find_if( m_queueFamilyProperties.begin(), m_queueFamilyProperties.end(),
        []( const auto& v ) { return ( v.queueFlags & VK_QUEUE_GRAPHICS_BIT ) != 0; } ) != m_queueFamilyProperties.end();
}

bool VlkPhysicalDevice::IsComputeCapable()
{
    return std::find_if( m_queueFamilyProperties.begin(), m_queueFamilyProperties.end(),
        []( const auto& v ) { return ( v.queueFlags & VK_QUEUE_GRAPHICS_BIT ) != 0; } ) != m_queueFamilyProperties.end();
}
