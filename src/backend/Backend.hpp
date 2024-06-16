#pragma once

#include <functional>
#include <vulkan/vulkan.h>

#include "util/NoCopy.hpp"

class VlkDevice;
class VlkSwapchain;

class Backend
{
public:
    virtual ~Backend() = default;

    NoCopy( Backend );

    virtual void VulkanInit() = 0;

    virtual void Run() = 0;
    virtual void Stop() = 0;

protected:
    Backend() = default;
};
