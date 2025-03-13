#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

#include "util/NoCopy.hpp"

class VlkPhysicalDevice;

enum class VlkInstanceType
{
    Wayland,
    Drm
};

class VlkInstance
{
public:
    VlkInstance( VlkInstanceType instanceType, bool enableValidation );
    ~VlkInstance();

    NoCopy( VlkInstance );

    [[nodiscard]] auto& QueryPhysicalDevices() const { return m_physicalDevices; }
    [[nodiscard]] auto Type() const { return m_instanceType; }
    [[nodiscard]] auto ApiVersion() const { return VK_API_VERSION_1_4; }

    operator VkInstance() const { return m_instance; }

private:
    void InitPhysicalDevices();

    VkInstance m_instance;
    VkDebugUtilsMessengerEXT m_debugMessenger;

    VlkInstanceType m_instanceType;

    std::vector<std::shared_ptr<VlkPhysicalDevice>> m_physicalDevices;
};
