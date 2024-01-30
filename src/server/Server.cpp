#include <stdlib.h>

#include "GpuState.hpp"
#include "Server.hpp"
#include "../backend/drm/BackendDrm.hpp"
#include "../backend/wayland/BackendWayland.hpp"
#include "../dbus/DbusSession.hpp"
#include "../plumbing/Display.hpp"
#include "../render/Background.hpp"
#include "../util/Logs.hpp"
#include "../vulkan/VlkInstance.hpp"

Server::Server()
    : m_dbusSession( std::make_unique<DbusSession>() )
{
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

    m_gpuState = std::make_unique<GpuState>( *m_vkInstance );

    if( waylandDpy )
    {
        m_backend = std::make_unique<BackendWayland>( *m_vkInstance, *m_gpuState );
    }
    else
    {
        m_backend = std::make_unique<BackendDrm>( *m_vkInstance, *m_dbusSession );
    }

    m_dpy = std::make_unique<Display>();
    setenv( "WAYLAND_DISPLAY", m_dpy->Socket(), 1 );

    m_renderables.emplace_back( std::make_shared<Background>( *m_gpuState ) );
}

Server::~Server()
{
}

void Server::Run()
{
    m_backend->Run( [this]{ Render(); } );
}

void Server::Render()
{
    m_gpuState->Connectors().Render( m_renderables );
}
