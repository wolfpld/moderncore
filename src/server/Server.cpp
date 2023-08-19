#include <stdlib.h>

#include "GpuDevices.hpp"
#include "GpuConnectors.hpp"
#include "Server.hpp"
#include "../backend/drm/BackendDrm.hpp"
#include "../backend/wayland/BackendWayland.hpp"
#include "../cursor/CursorLogic.hpp"
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

    m_gpus = std::make_unique<GpuDevices>( *m_vkInstance );
    m_connectors = std::make_unique<GpuConnectors>();

    if( waylandDpy )
    {
        m_backend = std::make_unique<BackendWayland>( *m_vkInstance, *m_gpus, *m_connectors );
    }
    else
    {
        m_backend = std::make_unique<BackendDrm>( *m_vkInstance, *m_dbusSession );
    }

    m_dpy = std::make_unique<Display>();
    setenv( "WAYLAND_DISPLAY", m_dpy->Socket(), 1 );

    /*
    m_backend->VulkanInit( *m_vkDevice, *m_renderPass, *m_swapchain );

    m_background = std::make_unique<Background>( *m_vkDevice, *m_renderPass, m_swapchain->GetWidth(), m_swapchain->GetHeight() );
    m_cursorLogic = std::make_unique<CursorLogic>();
    */
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
    m_connectors->Render();
}
