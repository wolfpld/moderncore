#include "Display.hpp"
#include "Panic.hpp"
#include "XdgShell.hpp"

extern "C" {
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
};

XdgShell::XdgShell( const Display& dpy )
    : m_xdgShell( wlr_xdg_shell_create( dpy, 3 ) )
    , m_newXdgSurface( m_xdgShell->events.new_surface, [this](auto v){ NewSurface( v ); } )
{
    CheckPanic( m_xdgShell, "Failed to create wlr_xdg_shell!" );
}

void XdgShell::NewSurface( wlr_xdg_surface* output )
{
    wlr_log( WLR_DEBUG, "New surface" );
}
