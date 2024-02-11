#include <assert.h>

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
#include "render/Background.hpp"
#include "util/Logs.hpp"
#include "util/TaskDispatch.hpp"
#include "vulkan/VlkInstance.hpp"

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

Server::Server( bool singleThread )
{
    ZoneScoped;

    assert( !s_instance );
    s_instance = this;

    mclog( LogLevel::Debug, "Main thread: %i", gettid() );

    auto dispatchThread = std::thread( [this, singleThread] {
        const auto cpus = singleThread ? 0 : std::thread::hardware_concurrency();
        m_dispatch = std::make_unique<TaskDispatch>( cpus == 0 ? 0 : cpus - 1, "Worker" );
    } );

    m_dbusSession = std::make_unique<DbusSession>();

    const auto waylandDpy = getenv( "WAYLAND_DISPLAY" );
    if( waylandDpy )
    {
        mclog( LogLevel::Info, "Running on Wayland display: %s", waylandDpy );
        m_vkInstance = std::make_unique<VlkInstance>( VlkInstanceType::Wayland );
    }
    else
    {
        m_vkInstance = std::make_unique<VlkInstance>( VlkInstanceType::Drm );
    }
    dispatchThread.join();

    SetupGpus( !waylandDpy );

    if( waylandDpy )
    {
        m_backend = std::make_unique<BackendWayland>();
    }
    else
    {
        m_backend = std::make_unique<BackendDrm>();
    }
    m_dispatch->Sync();

    m_backend->VulkanInit();

    m_dpy = std::make_unique<Display>();
    setenv( "WAYLAND_DISPLAY", m_dpy->Socket(), 1 );

    m_renderables.emplace_back( std::make_shared<Background>() );

    InitConnectorsInRenderables();
}

Server::~Server()
{
    assert( s_instance );
}

Server& Server::Instance()
{
    assert( s_instance );
    return *s_instance;
}

void Server::Run()
{
    m_backend->Run( [this]{ Render(); } );
}

void Server::Render()
{
    ZoneScoped;
    for( auto& gpu : m_gpus ) gpu->Render();
}

void Server::SetupGpus( bool skipSoftware )
{
    ZoneScoped;

    const auto& devices = m_vkInstance->QueryPhysicalDevices();
    std::vector<VkPhysicalDeviceProperties> props( devices.size() );

    mclog( LogLevel::Info, "Found %d physical devices", devices.size() );
    int idx = 0;
    int hw = 0;
    for( const auto& device : devices )
    {
        vkGetPhysicalDeviceProperties( device, &props[idx] );
        mclog( LogLevel::Info, "  %d: %s", idx, props[idx].deviceName );
        if( IsDeviceHardware( props[idx] ) ) hw++;
        idx++;
    }

    const auto num = skipSoftware ? hw : devices.size();
    m_gpus.resize( num );
    idx = 0;
    int vkIdx = 0;
    for( const auto& dev : devices )
    {
        if( !skipSoftware || IsDeviceHardware( props[vkIdx] ) )
        {
            m_dispatch->Queue( [this, dev, idx] {
                m_gpus[idx] = std::make_shared<GpuDevice>( *m_vkInstance, dev );
            } );
            idx++;
        }
        else
        {
            mclog( LogLevel::Info, "Skipping software device: %s", props[vkIdx].deviceName );
        }
        vkIdx++;
    }
}

void Server::InitConnectorsInRenderables()
{
    for( auto& renderable : m_renderables )
    {
        for( auto& gpu : m_gpus )
        {
            for( auto& connector : gpu->Connectors() )
            {
                m_dispatch->Queue( [renderable, connector] {
                    renderable->AddConnector( *connector );
                } );
            }
        }
    }
    m_dispatch->Sync();
}
