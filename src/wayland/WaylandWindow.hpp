#pragma once

#include <vulkan/vulkan.h>
#include <wayland-client.h>

#include "util/NoCopy.hpp"

#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#include "fractional-scale-v1-client-protocol.h"
#include "viewporter-client-protocol.h"

class VlkInstance;
class WaylandDisplay;

class WaylandWindow
{
public:
    struct Listener
    {
        void (*OnClose)( void* ptr, WaylandWindow* window );
    };

    WaylandWindow( WaylandDisplay& display, VlkInstance& vkInstance );
    virtual ~WaylandWindow();

    NoCopy( WaylandWindow );

    void SetListener( const Listener* listener, void* listenerPtr );

    [[nodiscard]] wl_surface* Surface() { return m_surface; }
    [[nodiscard]] xdg_toplevel* XdgToplevel() { return m_xdgToplevel; }
    [[nodiscard]] VkSurfaceKHR VkSurface() { return m_vkSurface; }

protected:
    [[nodiscard]] wp_fractional_scale_v1* FractionalScale() { return m_fractionalScale; }

private:
    void XdgSurfaceConfigure( struct xdg_surface *xdg_surface, uint32_t serial );

    void XdgToplevelConfigure( struct xdg_toplevel* toplevel, int32_t width, int32_t height, struct wl_array* states );
    void XdgToplevelClose( struct xdg_toplevel* toplevel );

    void DecorationConfigure( zxdg_toplevel_decoration_v1* tldec, uint32_t mode );

    void FractionalScalePreferredScale( wp_fractional_scale_v1* scale, uint32_t scaleValue );

    wl_surface* m_surface;
    xdg_surface* m_xdgSurface;
    xdg_toplevel* m_xdgToplevel;
    zxdg_toplevel_decoration_v1* m_xdgToplevelDecoration = nullptr;
    wp_fractional_scale_v1* m_fractionalScale = nullptr;
    wp_viewport* m_viewport = nullptr;

    VkSurfaceKHR m_vkSurface;
    VlkInstance& m_vkInstance;

    const Listener* m_listener = nullptr;
    void* m_listenerPtr;
};
