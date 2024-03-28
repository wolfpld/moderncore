#pragma once

#include <memory>
#include <stdint.h>
#include <vector>
#include <wayland-client.h>

#include "backend/Backend.hpp"
#include "util/NoCopy.hpp"
#include "util/RobinHood.hpp"

#include "fractional-scale-v1-client-protocol.h"
#include "viewporter-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

class WaylandOutput;
class WaylandSeat;
class WaylandWindow;
class VlkInstance;

class BackendWayland : public Backend
{
public:
    BackendWayland();
    ~BackendWayland() override;

    NoCopy( BackendWayland );

    void VulkanInit() override;

    void Run( const std::function<void()>& render ) override;
    void Stop() override;

    void PointerMotion( double x, double y );

    [[nodiscard]] const auto& OutputMap() const { return m_outputMap; }

private:
    void RegistryGlobal( wl_registry* reg, uint32_t name, const char* interface, uint32_t version );
    void RegistryGlobalRemove( wl_registry* reg, uint32_t name );

    void XdgWmPing( xdg_wm_base* shell, uint32_t serial );

    void OpenWindow( int physDev = -1 );

    wl_display* m_dpy;
    wl_compositor* m_compositor = nullptr;
    xdg_wm_base* m_xdgWmBase = nullptr;
    zxdg_decoration_manager_v1* m_decorationManager = nullptr;
    wp_fractional_scale_manager_v1* m_fractionalScaleManager = nullptr;
    wp_viewporter* m_viewporter = nullptr;

    std::unique_ptr<WaylandSeat> m_seat;
    std::vector<std::unique_ptr<WaylandWindow>> m_windows;

    unordered_flat_map<uint32_t, std::unique_ptr<WaylandOutput>> m_outputMap;

    bool m_keepRunning = true;
};
