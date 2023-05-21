#ifndef __XDGSHELL_HPP__
#define __XDGSHELL_HPP__

#include "Listener.hpp"

extern "C" {
    struct wlr_xdg_shell;
    struct wlr_xdg_surface;
};

class Display;

class XdgShell
{
public:
    explicit XdgShell( const Display& dpy );

private:
    void NewSurface( wlr_xdg_surface* output );

    wlr_xdg_shell* m_xdgShell;
    Listener<wlr_xdg_surface> m_newXdgSurface;
};

#endif
