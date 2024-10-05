#pragma once

#include <memory>
#include <vulkan/vulkan.h>

#include "util/NoCopy.hpp"
#include "vulkan/VlkDevice.hpp"

class VlkInstance;

class GpuDevice
{
public:
    GpuDevice( VlkInstance& instance, const std::shared_ptr<VlkPhysicalDevice>& physDev );
    ~GpuDevice();

    NoCopy( GpuDevice );

    [[nodiscard]] bool IsPresentSupported( VkSurfaceKHR surface ) const;

    [[nodiscard]] auto& Device() const { return m_device; }

private:
    std::shared_ptr<VlkDevice> m_device;
};
