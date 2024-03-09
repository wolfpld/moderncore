#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "util/NoCopy.hpp"

class VlkPhysicalDevice
{
public:
    explicit VlkPhysicalDevice( VkPhysicalDevice physDev );

    NoCopy( VlkPhysicalDevice );

    [[nodiscard]] auto& Properties() const { return m_properties; }
    [[nodiscard]] auto& QueueFamilyProperties() const { return m_queueFamilyProperties; }
    [[nodiscard]] auto& ExtensionProperties() const { return m_extensionProperties; }
    [[nodiscard]] auto& MemoryProperties() const { return m_memoryProperties; }

    [[nodiscard]] bool IsGraphicCapable() const;
    [[nodiscard]] bool IsComputeCapable() const;
    [[nodiscard]] bool IsSwapchainCapable() const;
    [[nodiscard]] bool HasPushDescriptor() const;
    [[nodiscard]] bool HasDynamicRendering() const;
    [[nodiscard]] bool HasCalibratedTimestamps() const;

    operator VkPhysicalDevice() const { return m_physDev; }

private:
    [[nodiscard]] bool IsExtensionAvailable( const char* extensionName ) const;

    VkPhysicalDevice m_physDev;
    VkPhysicalDeviceProperties m_properties;
    VkPhysicalDeviceMemoryProperties m_memoryProperties;

    VkPhysicalDeviceFeatures2 m_features;
    VkPhysicalDeviceDynamicRenderingFeatures m_dynamicRenderingFeatures;

    std::vector<VkQueueFamilyProperties> m_queueFamilyProperties;
    std::vector<VkExtensionProperties> m_extensionProperties;
};
