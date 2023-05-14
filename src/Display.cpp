#include <assert.h>
#include <wayland-server-core.h>

#include "Display.hpp"

Display::Display()
    : m_dpy( wl_display_create() )
{
    assert( m_dpy );
}

Display::~Display()
{
    wl_display_destroy( m_dpy );
}
