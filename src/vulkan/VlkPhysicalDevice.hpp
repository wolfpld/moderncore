#ifndef __VLKPHYSICALDEVICE_HPP__
#define __VLKPHYSICALDEVICE_HPP__

#include <vector>
#include <vulkan/vulkan.h>

class VlkPhysicalDevice
{
public:
    explicit VlkPhysicalDevice( VkPhysicalDevice physDev );

    [[nodiscard]] const std::vector<VkQueueFamilyProperties>& GetQueueFamilyProperties() const { return m_queueFamilyProperties; }
    [[nodiscard]] const std::vector<VkExtensionProperties>& GetExtensionProperties() const { return m_extensionProperties; }

    [[nodiscard]] bool IsGraphicCapable();
    [[nodiscard]] bool IsComputeCapable();

private:
    std::vector<VkQueueFamilyProperties> m_queueFamilyProperties;
    std::vector<VkExtensionProperties> m_extensionProperties;
};

#endif
