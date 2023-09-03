#pragma once

#include <functional>
#include <memory>
#include <wayland-client.h>
#include <vulkan/vulkan.h>

#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

#include "../../util/NoCopy.hpp"

class CursorLogic;
class GpuState;
class SoftwareCursor;
class VlkDevice;
class VlkInstance;

class WaylandWindow
{
public:
    struct Params
    {
        wl_compositor* compositor;
        xdg_wm_base* xdgWmBase;
        zxdg_decoration_manager_v1* decorationManager;
        wl_display* dpy;
        VlkInstance& vkInstance;
        GpuState& gpuState;
        std::function<void()> onClose;
    };

    explicit WaylandWindow( Params&& p );
    ~WaylandWindow();

    NoCopy( WaylandWindow );

    void Show( const std::function<void()>& render );
    void SetScale( int32_t scale );
    void RenderCursor( VkCommandBuffer cmdBuf, CursorLogic& cursorLogic );

    void PointerMotion( double x, double y );

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

    VlkInstance& m_vkInstance;
    std::shared_ptr<VlkDevice> m_vkDevice;

    std::unique_ptr<SoftwareCursor> m_cursor;

    GpuState& m_gpuState;
    uint32_t m_connectorId;
};
