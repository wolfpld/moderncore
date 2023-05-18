#include <wayland-server-core.h>

#include "Display.hpp"
#include "Panic.hpp"

Display::Display()
    : m_dpy( wl_display_create() )
{
    CheckPanic( m_dpy, "Failed to create wl_display!" );
}

Display::~Display()
{
    wl_display_destroy( m_dpy );
}
