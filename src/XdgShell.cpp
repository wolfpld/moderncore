#include "Display.hpp"
#include "Panic.hpp"
#include "XdgShell.hpp"

extern "C" {
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
};

static XdgShell* s_instance;

XdgShell::XdgShell( const Display& dpy )
    : m_xdgShell( wlr_xdg_shell_create( dpy, 3 ) )
{
    CheckPanic( m_xdgShell, "Failed to create wlr_xdg_shell!" );

    CheckPanic( !s_instance, "Creating a second instance of Output!" );
    s_instance = this;

    wl_list_init( &m_views );
    m_newXdgSurface.notify = []( struct wl_listener* listener, void* data ){ s_instance->NewSurface( (struct wlr_xdg_surface*)data ); };
    wl_signal_add( &m_xdgShell->events.new_surface, &m_newXdgSurface );
}

XdgShell::~XdgShell()
{
    wl_list_remove( &m_newXdgSurface.link );
    s_instance = nullptr;
}

void XdgShell::NewSurface( struct wlr_xdg_surface* output )
{
    wlr_log( WLR_DEBUG, "New surface" );
}
