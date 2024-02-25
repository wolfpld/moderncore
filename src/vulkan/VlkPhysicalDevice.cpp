#include <algorithm>
#include <string.h>
#include <tracy/Tracy.hpp>

#include "VlkPhysicalDevice.hpp"

VlkPhysicalDevice::VlkPhysicalDevice( VkPhysicalDevice physDev )
    : m_physDev( physDev )
{
    ZoneScoped;

    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties( physDev, &count, nullptr );
    m_queueFamilyProperties.resize( count );
    vkGetPhysicalDeviceQueueFamilyProperties( physDev, &count, m_queueFamilyProperties.data() );

    vkEnumerateDeviceExtensionProperties( physDev, nullptr, &count, nullptr );
    m_extensionProperties.resize( count );
    vkEnumerateDeviceExtensionProperties( physDev, nullptr, &count, m_extensionProperties.data() );

    std::sort( m_extensionProperties.begin(), m_extensionProperties.end(), []( const auto& lhs, const auto& rhs ) { return strcmp( lhs.extensionName, rhs.extensionName ) < 0; } );
}

bool VlkPhysicalDevice::IsGraphicCapable()
{
    ZoneScoped;
    return std::any_of( m_queueFamilyProperties.begin(), m_queueFamilyProperties.end(),
        []( const auto& v ) { return ( v.queueFlags & VK_QUEUE_GRAPHICS_BIT ) != 0; } );
}

bool VlkPhysicalDevice::IsComputeCapable()
{
    ZoneScoped;
    return std::any_of( m_queueFamilyProperties.begin(), m_queueFamilyProperties.end(),
        []( const auto& v ) { return ( v.queueFlags & VK_QUEUE_GRAPHICS_BIT ) != 0; } );
}

bool VlkPhysicalDevice::IsSwapchainCapable()
{
    return IsExtensionAvailable( VK_KHR_SWAPCHAIN_EXTENSION_NAME );
}

bool VlkPhysicalDevice::HasPushDescriptor()
{
    return IsExtensionAvailable( VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME );
}

bool VlkPhysicalDevice::IsExtensionAvailable( const char* extensionName ) const
{
    ZoneScoped;
    ZoneText( extensionName, strlen( extensionName ) );

    auto it = std::lower_bound( m_extensionProperties.begin(), m_extensionProperties.end(), extensionName,
        []( const auto& lhs, const auto& rhs ) { return strcmp( lhs.extensionName, rhs ) < 0; } );
    return it != m_extensionProperties.end() && strcmp( it->extensionName, extensionName ) == 0;
}
