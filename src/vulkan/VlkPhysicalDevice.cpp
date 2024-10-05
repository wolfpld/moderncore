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

    vkGetPhysicalDeviceProperties( physDev, &m_properties );
    vkGetPhysicalDeviceMemoryProperties( physDev, &m_memoryProperties );

    m_dynamicRenderingFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES };

    m_features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    m_features.pNext = &m_dynamicRenderingFeatures;

    vkGetPhysicalDeviceFeatures2( physDev, &m_features );
}

bool VlkPhysicalDevice::IsGraphicCapable() const
{
    ZoneScoped;
    return std::any_of( m_queueFamilyProperties.begin(), m_queueFamilyProperties.end(),
        []( const auto& v ) { return ( v.queueFlags & VK_QUEUE_GRAPHICS_BIT ) != 0; } );
}

bool VlkPhysicalDevice::IsComputeCapable() const
{
    ZoneScoped;
    return std::any_of( m_queueFamilyProperties.begin(), m_queueFamilyProperties.end(),
        []( const auto& v ) { return ( v.queueFlags & VK_QUEUE_GRAPHICS_BIT ) != 0; } );
}

bool VlkPhysicalDevice::IsSwapchainCapable() const
{
    return IsExtensionAvailable( VK_KHR_SWAPCHAIN_EXTENSION_NAME );
}

bool VlkPhysicalDevice::HasPushDescriptor() const
{
    return IsExtensionAvailable( VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME );
}

bool VlkPhysicalDevice::HasDynamicRendering() const
{
    return m_dynamicRenderingFeatures.dynamicRendering == VK_TRUE;
}

bool VlkPhysicalDevice::HasCalibratedTimestamps() const
{
    return IsExtensionAvailable( VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME );
}

bool VlkPhysicalDevice::HasPciBusInfo() const
{
    return IsExtensionAvailable( VK_EXT_PCI_BUS_INFO_EXTENSION_NAME );
}

bool VlkPhysicalDevice::IsDeviceHardware() const
{
    return m_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
           m_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
}

bool VlkPhysicalDevice::IsExtensionAvailable( const char* extensionName ) const
{
    ZoneScoped;
    ZoneText( extensionName, strlen( extensionName ) );

    auto it = std::lower_bound( m_extensionProperties.begin(), m_extensionProperties.end(), extensionName,
        []( const auto& lhs, const auto& rhs ) { return strcmp( lhs.extensionName, rhs ) < 0; } );
    return it != m_extensionProperties.end() && strcmp( it->extensionName, extensionName ) == 0;
}
