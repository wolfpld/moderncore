#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

class VlkDevice;
class VlkInstance;

class GpuDevices
{
    struct Gpu
    {
        VkPhysicalDevice physDev;
        std::shared_ptr<VlkDevice> device;
    };

public:
    explicit GpuDevices( VlkInstance& instance );
    ~GpuDevices();

    const std::shared_ptr<VlkDevice>& Get( VkPhysicalDevice physDev ) const;
    std::shared_ptr<VlkDevice> Add( VkPhysicalDevice physDev, VkSurfaceKHR surface );

private:
    VlkInstance& m_instance;

    std::vector<Gpu> m_gpus;
};
