#include <algorithm>
#include <string.h>
#include <ranges>
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

    std::ranges::sort( m_extensionProperties, []( const auto& lhs, const auto& rhs ) { return strcmp( lhs.extensionName, rhs.extensionName ) < 0; } );

    vkGetPhysicalDeviceProperties( physDev, &m_properties );
    vkGetPhysicalDeviceMemoryProperties( physDev, &m_memoryProperties );

    m_features13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES
    };
    m_features12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &m_features13
    };
    m_features11 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext = &m_features12
    };
    m_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &m_features11
    };

    vkGetPhysicalDeviceFeatures2( physDev, &m_features );
}

bool VlkPhysicalDevice::IsGraphicCapable() const
{
    ZoneScoped;
    return std::ranges::any_of( m_queueFamilyProperties, []( const auto& v ) { return ( v.queueFlags & VK_QUEUE_GRAPHICS_BIT ) != 0; } );
}

bool VlkPhysicalDevice::IsComputeCapable() const
{
    ZoneScoped;
    return std::ranges::any_of( m_queueFamilyProperties, []( const auto& v ) { return ( v.queueFlags & VK_QUEUE_GRAPHICS_BIT ) != 0; } );
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
    return m_features13.dynamicRendering == VK_TRUE;
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
