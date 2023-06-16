#ifndef __WAYLANDWINDOW_HPP__
#define __WAYLANDWINDOW_HPP__

#include <functional>
#include <memory>
#include <wayland-client.h>
#include <vulkan/vulkan.h>

#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

class SoftwareCursor;
class VlkDevice;

class WaylandWindow
{
public:
    WaylandWindow( wl_compositor* compositor, xdg_wm_base* xdgWmBase, zxdg_decoration_manager_v1* decorationManager, wl_display* dpy, VkInstance vkInstance, std::function<void()> onClose );
    ~WaylandWindow();

    void VulkanInit( VlkDevice& device, VkRenderPass renderPass, uint32_t width, uint32_t height );

    void Show( const std::function<void()>& render );
    void RenderCursor( VkCommandBuffer cmdBuf );

    void PointerMotion( double x, double y );

    operator VkSurfaceKHR() const { return m_vkSurface; }

private:
    void XdgSurfaceConfigure( struct xdg_surface *xdg_surface, uint32_t serial );

    void XdgToplevelConfigure( struct xdg_toplevel* toplevel, int32_t width, int32_t height, struct wl_array* states );
    void XdgToplevelClose( struct xdg_toplevel* toplevel );

    void FrameDone( struct wl_callback* cb, uint32_t time );

    wl_surface* m_surface;
    xdg_surface* m_xdgSurface;
    xdg_toplevel* m_xdgToplevel;
    VkSurfaceKHR m_vkSurface;

    std::function<void()> m_onClose;
    std::function<void()> m_onRender;

    VkInstance m_vkInstance;

    std::unique_ptr<SoftwareCursor> m_cursor;
};

#endif
