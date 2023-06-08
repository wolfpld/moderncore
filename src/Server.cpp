#include <stdlib.h>

#include "Server.hpp"
#include "plumbing/Display.hpp"

Server::Server()
{
    setenv( "WAYLAND_DISPLAY", m_dpy.Socket(), 1 );
    m_dpy.Run();
}
