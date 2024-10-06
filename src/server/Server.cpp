#include <stdlib.h>
#include <tracy/Tracy.hpp>
#include <thread>

#include "Server.hpp"
#include "backend/drm/BackendDrm.hpp"
#include "backend/wayland/BackendWayland.hpp"
#include "dbus/DbusSession.hpp"
#include "plumbing/Display.hpp"
#include "util/Logs.hpp"
#include "util/Panic.hpp"
#include "vulkan/VlkInstance.hpp"


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
