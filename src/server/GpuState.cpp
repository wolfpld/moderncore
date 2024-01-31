#include "GpuState.hpp"
#include "util/Logs.hpp"
#include "vulkan/VlkInstance.hpp"

GpuState::GpuState( VlkInstance& instance )
    : m_devices( instance )
{
    auto devices = instance.QueryPhysicalDevices();
    mclog( LogLevel::Info, "Found %d physical devices", devices.size() );
    int idx = 0;
    for( auto& device : devices )
    {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties( device, &properties );
        mclog( LogLevel::Info, "  %d: %s", idx++, properties.deviceName );
    }
}
