#ifndef __XDGSHELL_HPP__
#define __XDGSHELL_HPP__

#include <wayland-server-core.h>

extern "C" {
    struct wlr_xdg_shell;
};

class Display;

class XdgShell
{
public:
    explicit XdgShell( const Display& dpy );
    ~XdgShell();

    void NewSurface( struct wlr_xdg_surface* output );

private:
    struct wlr_xdg_shell* m_xdgShell;
    struct wl_listener m_newXdgSurface;
    struct wl_list m_views;
};

#endif
