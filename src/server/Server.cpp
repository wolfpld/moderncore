#include <stdlib.h>
#include <tracy/Tracy.hpp>
#include <thread>
#include <unistd.h>

#include "GpuDevice.hpp"
#include "Server.hpp"
#include "backend/drm/BackendDrm.hpp"
#include "backend/wayland/BackendWayland.hpp"
#include "dbus/DbusSession.hpp"
#include "plumbing/Display.hpp"
#include "util/Logs.hpp"
#include "util/Panic.hpp"
#include "vulkan/VlkInstance.hpp"
#include "vulkan/VlkPhysicalDevice.hpp"

namespace
{
bool IsDeviceHardware( const VkPhysicalDeviceProperties& properties )
{
    return properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
           properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
}
}

namespace
{
Server* s_instance = nullptr;
}

Server::Server( bool enableValidation )
{
    ZoneScoped;

    CheckPanic( !s_instance, "Server already exists" );
    s_instance = this;

    mclog( LogLevel::Debug, "Main thread: %i", gettid() );

    std::thread vulkanThread;
    const auto waylandDpy = getenv( "WAYLAND_DISPLAY" );
    if( waylandDpy )
    {
        mclog( LogLevel::Info, "Running on Wayland display: %s", waylandDpy );
        vulkanThread = std::thread( [this, enableValidation] {
            m_vkInstance = std::make_unique<VlkInstance>( VlkInstanceType::Wayland, enableValidation );
        } );
        m_backend = std::make_unique<BackendWayland>();
    }
    else
    {
        vulkanThread = std::thread( [this, enableValidation] {
            m_vkInstance = std::make_unique<VlkInstance>( VlkInstanceType::Drm, enableValidation );
        } );
        m_backend = std::make_unique<BackendDrm>();
    }

    m_dbusSession = std::make_unique<DbusSession>();
    vulkanThread.join();

    m_vkInstance->InitPhysicalDevices();

    SetupGpus( !waylandDpy );

    m_backend->VulkanInit();

    m_dpy = std::make_unique<Display>();
    setenv( "WAYLAND_DISPLAY", m_dpy->Socket(), 1 );
}

Server::~Server()
{
    CheckPanic( s_instance, "Server does not exist" );
}

Server& Server::Instance()
{
    CheckPanic( s_instance, "Server does not exist" );
    return *s_instance;
}

void Server::Run()
{
    m_backend->Run();
}

void Server::SetupGpus( bool skipSoftware )
{
    ZoneScoped;

    const auto& devices = m_vkInstance->QueryPhysicalDevices();
    CheckPanic( !devices.empty(), "No physical devices found" );

    mclog( LogLevel::Info, "Found %d physical devices", devices.size() );
    int idx = 0;
    int hw = 0;
    for( const auto& dev : devices )
    {
        auto& props = dev->Properties();
        mclog( LogLevel::Info, "  %d: %s", idx, props.deviceName );
        if( IsDeviceHardware( props ) ) hw++;
        idx++;
    }

    const auto num = skipSoftware ? hw : devices.size();
    m_gpus.resize( num );
    idx = 0;
    for( const auto& dev : devices )
    {
        auto& props = dev->Properties();
        if( !skipSoftware || IsDeviceHardware( props ) )
        {
            try
            {
                m_gpus[idx] = std::make_shared<GpuDevice>( *m_vkInstance, dev );
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
