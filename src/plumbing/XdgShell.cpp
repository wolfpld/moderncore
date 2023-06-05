#include "Display.hpp"
#include "XdgShell.hpp"
#include "../util/Logs.hpp"
#include "../util/Panic.hpp"

extern "C" {
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_xdg_shell.h>
};

XdgShell::XdgShell( const Display& dpy )
    : m_xdgShell( wlr_xdg_shell_create( dpy, 3 ) )
    , m_newXdgSurface( m_xdgShell->events.new_surface, [this](auto v){ NewSurface( v ); } )
{
    CheckPanic( m_xdgShell, "Failed to create wlr_xdg_shell!" );
}

void XdgShell::NewSurface( wlr_xdg_surface* output )
{
    mclog( LogLevel::Debug, "New surface" );
}
