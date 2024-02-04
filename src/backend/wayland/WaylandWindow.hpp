#pragma once

#include <functional>
#include <memory>
#include <wayland-client.h>
#include <vulkan/vulkan.h>

#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

#include "util/NoCopy.hpp"
#include "util/RobinHood.hpp"

class BackendWayland;
class CursorLogic;
class GpuDevice;
class SoftwareCursor;
class VlkInstance;

class WaylandWindow
{
public:
    struct Params
    {
        int physDev;
        wl_compositor* compositor;
        xdg_wm_base* xdgWmBase;
        zxdg_decoration_manager_v1* decorationManager;
        wl_display* dpy;
        std::function<void()> onClose;
        BackendWayland& backend;
    };

    explicit WaylandWindow( Params&& p );
    ~WaylandWindow();

    NoCopy( WaylandWindow );

    void Show( const std::function<void()>& render );
    void RenderCursor( VkCommandBuffer cmdBuf, CursorLogic& cursorLogic );

    void PointerMotion( double x, double y );

private:
    void Enter( struct wl_surface* surface, struct wl_output* output );
    void Leave( struct wl_surface* surface, struct wl_output* output );

    void XdgSurfaceConfigure( struct xdg_surface *xdg_surface, uint32_t serial );

    void XdgToplevelConfigure( struct xdg_toplevel* toplevel, int32_t width, int32_t height, struct wl_array* states );
    void XdgToplevelClose( struct xdg_toplevel* toplevel );

    void FrameDone( struct wl_callback* cb, uint32_t time );

    void DecorationConfigure( zxdg_toplevel_decoration_v1* tldec, uint32_t mode );

    wl_surface* m_surface;
    xdg_surface* m_xdgSurface;
    xdg_toplevel* m_xdgToplevel;
    zxdg_toplevel_decoration_v1* m_xdgToplevelDecoration = nullptr;
    VkSurfaceKHR m_vkSurface;

    std::function<void()> m_onClose;
    std::function<void()> m_onRender;

    BackendWayland& m_backend;
    std::shared_ptr<GpuDevice> m_gpu;

    std::unique_ptr<SoftwareCursor> m_cursor;

    uint32_t m_connectorId;

    unordered_flat_set<uint32_t> m_outputs;
    int m_scale = 0;
};
