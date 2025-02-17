#pragma once

#include <memory>
#include <vector>
#include <wayland-client.h>

#include "util/NoCopy.hpp"

#include "wayland-cursor-shape-client-protocol.h"
#include "wayland-fractional-scale-client-protocol.h"
#include "wayland-viewporter-client-protocol.h"
#include "wayland-xdg-decoration-client-protocol.h"
#include "wayland-xdg-shell-client-protocol.h"
#include "wayland-xdg-toplevel-icon-client-protocol.h"

class WaylandSeat;

class WaylandDisplay
{
public:
    WaylandDisplay();
    ~WaylandDisplay();
    NoCopy( WaylandDisplay );

    void Connect();
    void Roundtrip();

    void Run();
    void Stop() { m_keepRunning = false; }

    [[nodiscard]] wl_display* Display() { return m_dpy; }
    [[nodiscard]] wl_compositor* Compositor() { return m_compositor; }
    [[nodiscard]] xdg_wm_base* XdgWmBase() { return m_xdgWmBase; }
    [[nodiscard]] zxdg_decoration_manager_v1* DecorationManager() { return m_decorationManager; }
    [[nodiscard]] wp_fractional_scale_manager_v1* FractionalScaleManager() { return m_fractionalScaleManager; }
    [[nodiscard]] wp_viewporter* Viewporter() { return m_viewporter; }
    [[nodiscard]] xdg_toplevel_icon_manager_v1* IconManager() { return m_iconManager; }

    [[nodiscard]] const std::vector<int32_t>& IconSizes() const { return m_iconSizes; }

protected:
    void RegistryGlobal( wl_registry* reg, uint32_t name, const char* interface, uint32_t version );
    void RegistryGlobalRemove( wl_registry* reg, uint32_t name );

private:
    void RegistryGlobalShim( wl_registry* reg, uint32_t name, const char* interface, uint32_t version );

    void XdgWmPing( xdg_wm_base* shell, uint32_t serial );
    void IconManagerSize( xdg_toplevel_icon_manager_v1* manager, int32_t size );

    wl_display* m_dpy = nullptr;
    wl_compositor* m_compositor = nullptr;
    xdg_wm_base* m_xdgWmBase = nullptr;
    std::unique_ptr<WaylandSeat> m_seat;
    zxdg_decoration_manager_v1* m_decorationManager = nullptr;
    wp_fractional_scale_manager_v1* m_fractionalScaleManager = nullptr;
    wp_viewporter* m_viewporter = nullptr;
    wp_cursor_shape_manager_v1* m_cursorShapeManager = nullptr;
    xdg_toplevel_icon_manager_v1* m_iconManager = nullptr;

    bool m_keepRunning = true;

    std::vector<int32_t> m_iconSizes;
};
