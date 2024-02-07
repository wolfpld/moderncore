#include <assert.h>

#include <tracy/Tracy.hpp>
#include <stdlib.h>

#include "GpuDevice.hpp"
#include "Server.hpp"
#include "backend/drm/BackendDrm.hpp"
#include "backend/wayland/BackendWayland.hpp"
#include "dbus/DbusSession.hpp"
#include "plumbing/Display.hpp"
#include "render/Background.hpp"
#include "util/Logs.hpp"
#include "vulkan/VlkInstance.hpp"

namespace
{
Server* s_instance = nullptr;
}

Server::Server()
    : m_dbusSession( std::make_unique<DbusSession>() )
{
    ZoneScoped;

    assert( !s_instance );
    s_instance = this;

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

    //m_renderables.emplace_back( std::make_shared<Background>( *m_gpuState ) );
}

Server::~Server()
{
    assert( s_instance );
    s_instance = nullptr;
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

    for( const auto& dev : devices )
    {
        m_gpus.emplace_back( std::make_shared<GpuDevice>( *m_vkInstance, dev ) );
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
                renderable->AddConnector( *connector );
            }
        }
    }
}
