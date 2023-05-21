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

    m_newXdgSurface.notify = []( wl_listener*, void* data ){ s_instance->NewSurface( (wlr_xdg_surface*)data ); };
    wl_signal_add( &m_xdgShell->events.new_surface, &m_newXdgSurface );
}

XdgShell::~XdgShell()
{
    wl_list_remove( &m_newXdgSurface.link );
    s_instance = nullptr;
}

void XdgShell::NewSurface( wlr_xdg_surface* output )
{
    wlr_log( WLR_DEBUG, "New surface" );
}
