#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "util/NoCopy.hpp"

enum class VlkInstanceType
{
    Wayland,
    Drm
};

class VlkInstance
{
public:
    explicit VlkInstance( VlkInstanceType instanceType );
    ~VlkInstance();

    NoCopy( VlkInstance );

    [[nodiscard]] std::vector<VkPhysicalDevice> QueryPhysicalDevices() const;

    operator VkInstance() const { return m_instance; }

private:
    VkInstance m_instance;
    VkDebugUtilsMessengerEXT m_debugMessenger;
};
