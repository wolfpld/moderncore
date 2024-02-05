#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "util/NoCopy.hpp"
#include "vulkan/VlkDevice.hpp"

class Connector;

class GpuDevice
{
public:
    explicit GpuDevice( VkInstance instance, VkPhysicalDevice physDev );
    ~GpuDevice();

    NoCopy( GpuDevice );

    void Render();

    void AddConnector( std::shared_ptr<Connector> connector );
    void RemoveConnector( const std::shared_ptr<Connector>& connector );

    [[nodiscard]] bool IsPresentSupported( VkSurfaceKHR surface ) const;

    [[nodiscard]] auto& Device() { return m_device; }
    [[nodiscard]] auto& Device() const { return m_device; }

private:
    VlkDevice m_device;

    std::vector<std::shared_ptr<Connector>> m_connectors;
};
