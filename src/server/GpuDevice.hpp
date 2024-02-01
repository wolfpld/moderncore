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

private:
    VlkDevice m_device;
};
