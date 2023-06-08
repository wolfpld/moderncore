#include <stdlib.h>

#include "Server.hpp"
#include "backend/wayland/BackendWayland.hpp"
#include "plumbing/Display.hpp"
#include "util/Logs.hpp"
#include "util/Panic.hpp"
#include "vulkan/PhysDevSel.hpp"
#include "vulkan/VlkInstance.hpp"

Server::Server()
{
    const auto waylandDpy = getenv( "WAYLAND_DISPLAY" );
    if( waylandDpy )
    {
        mclog( LogLevel::Info, "Running on Wayland display: %s", waylandDpy );

        m_vkInstance = std::make_unique<VlkInstance>( VlkInstanceType::Wayland );
        m_backend = std::make_unique<BackendWayland>( *m_vkInstance );
    }
    else
    {
        mclog( LogLevel::Fatal, "Direct rendering is not implemented yet" );
        abort();
    }

    auto dev = PhysDevSel::PickBest( m_vkInstance->QueryPhysicalDevices(), *m_backend, PhysDevSel::RequireGraphic );
    CheckPanic( dev, "No suitable GPU found" );

    m_dpy = std::make_unique<Display>();
    setenv( "WAYLAND_DISPLAY", m_dpy->Socket(), 1 );

    m_backend->Run();
}

Server::~Server()
{
}
