#include <tracy/Tracy.hpp>

#include "Backend.hpp"
#include "GpuDevice.hpp"
#include "util/Logs.hpp"
#include "util/Panic.hpp"
#include "vulkan/VlkInstance.hpp"

void Backend::SetupGpuDevices( VlkInstance& vkInstance, bool skipSoftware )
{
    ZoneScoped;

    const auto& devices = vkInstance.QueryPhysicalDevices();
    CheckPanic( !devices.empty(), "No physical devices found" );
    mclog( LogLevel::Info, "Found %d physical devices", devices.size() );

    int idx = 0;
    int hw = 0;
    for( const auto& dev : devices )
    {
        auto& props = dev->Properties();
        mclog( LogLevel::Info, "  %d: %s", idx, props.deviceName );
        if( dev->IsDeviceHardware() ) hw++;
        idx++;
    }

    const auto num = skipSoftware ? hw : devices.size();
    m_gpus.resize( num );
    idx = 0;
    for( const auto& dev : devices )
    {
        auto& props = dev->Properties();
        if( !skipSoftware || dev->IsDeviceHardware() )
        {
            try
            {
                m_gpus[idx] = std::make_shared<GpuDevice>( vkInstance, dev );
            }
            catch( const std::exception& e )
            {
                mclog( LogLevel::Fatal, "Failed to initialize GPU: %s", e.what() );
            }
            idx++;
        }
        else
        {
            mclog( LogLevel::Info, "Skipping software device: %s", props.deviceName );
        }
    }
}
