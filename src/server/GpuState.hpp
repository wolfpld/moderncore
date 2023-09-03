#pragma once

#include <memory>

#include "GpuDevices.hpp"
#include "GpuConnectors.hpp"
#include "../util/NoCopy.hpp"

class VlkInstance;

class GpuState
{
public:
    explicit GpuState( VlkInstance& instance ) : m_devices( instance ) {}
    NoCopy( GpuState );

    [[nodiscard]] GpuDevices& Devices() { return m_devices; }
    [[nodiscard]] const GpuDevices& Devices() const { return m_devices; }
    [[nodiscard]] GpuConnectors& Connectors() { return m_connectors; }
    [[nodiscard]] const GpuConnectors& Connectors() const { return m_connectors; }

private:
    GpuDevices m_devices;
    GpuConnectors m_connectors;
};
