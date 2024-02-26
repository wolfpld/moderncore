#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

#include "util/NoCopy.hpp"

class TaskDispatch;
class VlkPhysicalDevice;

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

    void InitPhysicalDevices( TaskDispatch& dispatch );

    [[nodiscard]] auto& QueryPhysicalDevices() const { return m_physicalDevices; }
    [[nodiscard]] auto Type() const { return m_instanceType; }

    operator VkInstance() const { return m_instance; }

private:
    VkInstance m_instance;
    VkDebugUtilsMessengerEXT m_debugMessenger;

    VlkInstanceType m_instanceType;

    std::vector<std::shared_ptr<VlkPhysicalDevice>> m_physicalDevices;
};
