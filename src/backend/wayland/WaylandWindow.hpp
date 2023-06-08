#ifndef __WAYLANDWINDOW_HPP__
#define __WAYLANDWINDOW_HPP__

#include <functional>
#include <wayland-client.h>
#include <vulkan/vulkan.h>

#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

class WaylandWindow
{
public:
    WaylandWindow( wl_compositor* compositor, xdg_wm_base* xdgWmBase, zxdg_decoration_manager_v1* decorationManager, wl_display* dpy, VkInstance vkInstance, std::function<void()> onClose );
    ~WaylandWindow();

    void Show();

    operator VkSurfaceKHR() const { return m_vkSurface; }

private:
    void XdgSurfaceConfigure( struct xdg_surface *xdg_surface, uint32_t serial );

    void XdgToplevelConfigure( struct xdg_toplevel* toplevel, int32_t width, int32_t height, struct wl_array* states );
    void XdgToplevelClose( struct xdg_toplevel* toplevel );

    wl_surface* m_surface;
    xdg_surface* m_xdgSurface;
    xdg_toplevel* m_xdgToplevel;
    VkSurfaceKHR m_vkSurface;

    std::function<void()> m_onClose;

    VkInstance m_vkInstance;
};

#endif
