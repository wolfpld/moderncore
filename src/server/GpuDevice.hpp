#pragma once

#include <vulkan/vulkan.h>

#include "util/NoCopy.hpp"
#include "vulkan/VlkDevice.hpp"

class GpuDevice
{
public:
    explicit GpuDevice( VkInstance instance, VkPhysicalDevice physDev );
    ~GpuDevice();

    NoCopy( GpuDevice );

    [[nodiscard]] auto& Device() const { return m_device; }

private:
    VlkDevice m_device;
};
