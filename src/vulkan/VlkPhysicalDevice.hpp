#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "util/NoCopy.hpp"

class VlkPhysicalDevice
{
public:
    explicit VlkPhysicalDevice( VkPhysicalDevice physDev );

    NoCopy( VlkPhysicalDevice );

    [[nodiscard]] const std::vector<VkQueueFamilyProperties>& GetQueueFamilyProperties() const { return m_queueFamilyProperties; }
    [[nodiscard]] const std::vector<VkExtensionProperties>& GetExtensionProperties() const { return m_extensionProperties; }

    [[nodiscard]] bool IsGraphicCapable();
    [[nodiscard]] bool IsComputeCapable();
    [[nodiscard]] bool IsSwapchainCapable();
    [[nodiscard]] bool HasPushDescriptor();

    operator VkPhysicalDevice() const { return m_physDev; }

private:
    [[nodiscard]] bool IsExtensionAvailable( const char* extensionName ) const;

    VkPhysicalDevice m_physDev;

    std::vector<VkQueueFamilyProperties> m_queueFamilyProperties;
    std::vector<VkExtensionProperties> m_extensionProperties;
};
