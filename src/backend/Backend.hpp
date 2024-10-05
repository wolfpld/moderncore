#pragma once

#include <memory>
#include <vector>

#include "util/NoCopy.hpp"

class GpuDevice;
class VlkInstance;

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

    void SetupGpuDevices( VlkInstance& instance, bool skipSoftware );

    std::vector<std::shared_ptr<GpuDevice>> m_gpus;
};
