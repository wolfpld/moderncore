#include <assert.h>

#include <stdlib.h>
#include <tracy/Tracy.hpp>
#include <thread>

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
Server* s_instance = nullptr;
}

Server::Server()
{
    ZoneScoped;

    assert( !s_instance );
    s_instance = this;

    auto dispatchThread = std::thread( [this] {
        const auto cpus = std::thread::hardware_concurrency();
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

    SetupGpus();

    if( waylandDpy )
    {
        m_backend = std::make_unique<BackendWayland>();
    }
    else
    {
        m_backend = std::make_unique<BackendDrm>( *m_vkInstance, *m_dbusSession );
    }

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

void Server::SetupGpus()
{
    ZoneScoped;

    const auto& devices = m_vkInstance->QueryPhysicalDevices();

    mclog( LogLevel::Info, "Found %d physical devices", devices.size() );
    int idx = 0;
    for( const auto& device : devices )
    {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties( device, &properties );
        mclog( LogLevel::Info, "  %d: %s", idx++, properties.deviceName );
    }

    m_gpus.resize( devices.size() );
    idx = 0;
    for( const auto& dev : devices )
    {
        m_dispatch->Queue( [this, dev, idx] {
            m_gpus[idx] = std::make_shared<GpuDevice>( *m_vkInstance, dev );
        } );
        idx++;
    }
    m_dispatch->Sync();
}

void Server::InitConnectorsInRenderables()
{
    for( auto& renderable : m_renderables )
    {
        for( auto& gpu : m_gpus )
        {
            for( auto& connector : gpu->Connectors() )
            {
                renderable->AddConnector( *connector );
            }
        }
    }
}
