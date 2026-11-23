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

    m_features14 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES
    };
    m_features13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &m_features14
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
    return std::ranges::any_of( m_queueFamilyProperties, []( const auto& v ) { return ( v.queueFlags & VK_QUEUE_COMPUTE_BIT ) != 0; } );
}

bool VlkPhysicalDevice::IsSwapchainCapable() const
{
    return IsExtensionAvailable( VK_KHR_SWAPCHAIN_EXTENSION_NAME );
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

bool VlkPhysicalDevice::HasHostImageCopy() const
{
    return m_features14.hostImageCopy == VK_TRUE;
}

bool VlkPhysicalDevice::HasHostImageCopyStorage( VkFormat storageFormat, VkFormat viewFormat ) const
{
    const VkImageFormatListCreateInfo viewList = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO,
        .viewFormatCount = 1,
        .pViewFormats = &viewFormat
    };
    const VkPhysicalDeviceImageFormatInfo2 info = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
        .pNext = &viewList,
        .format = storageFormat,
        .type = VK_IMAGE_TYPE_2D,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_HOST_TRANSFER_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
        .flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT
    };
    VkImageFormatProperties2 props = { .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2 };
    return vkGetPhysicalDeviceImageFormatProperties2( m_physDev, &info, &props ) == VK_SUCCESS;
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
