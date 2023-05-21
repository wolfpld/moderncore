#ifndef __XDGSHELL_HPP__
#define __XDGSHELL_HPP__

#include <wayland-server-core.h>

extern "C" {
    struct wlr_xdg_shell;
    struct wlr_xdg_surface;
};

class Display;

class XdgShell
{
public:
    explicit XdgShell( const Display& dpy );
    ~XdgShell();

private:
    void NewSurface( wlr_xdg_surface* output );

    wlr_xdg_shell* m_xdgShell;
    wl_listener m_newXdgSurface;
};

#endif
