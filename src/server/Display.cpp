#include <wayland-server-core.h>

#include "Display.hpp"
#include "util/Logs.hpp"
#include "util/Panic.hpp"


Display::Display()
    : m_dpy( wl_display_create() )
    , m_socket( wl_display_add_socket_auto( m_dpy ) )
{
    CheckPanic( m_dpy, "Failed to create wl_display!" );
    CheckPanic( m_socket, "Failed to initialize a socket!" );

    mclog( LogLevel::Info, "Wayland socket: %s", m_socket );
}

Display::~Display()
{
    wl_display_destroy_clients( m_dpy );
    wl_display_destroy( m_dpy );
}

void Display::Run()
{
    wl_display_run( m_dpy );
}

void Display::Terminate()
{
    wl_display_terminate( m_dpy );
}
