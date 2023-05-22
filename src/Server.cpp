#include <stdlib.h>

#include "Display.hpp"
#include "Server.hpp"

Server::Server()
    : m_backend( m_dpy )
    , m_renderer( m_backend, m_dpy )
    , m_allocator( m_backend, m_renderer )
    , m_compositor( m_dpy, m_renderer )
    , m_subcompositor( m_dpy )
    , m_ddm( m_dpy )
    , m_output( m_backend, m_allocator, m_renderer, m_scene )
    , m_xdgShell( m_dpy )
    , m_seat( m_dpy, m_backend, m_output )
{
    m_backend.Start();
    setenv( "WAYLAND_DISPLAY", m_dpy.Socket(), 1 );
    m_dpy.Run();
}
