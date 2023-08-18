#pragma once

#include <memory>
#include <stdint.h>
#include <wayland-client.h>

#include "../Backend.hpp"
#include "../../util/RobinHood.hpp"

#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

class GpuDevices;
class GpuConnectors;
class WaylandOutput;
class WaylandSeat;
class WaylandWindow;
class VlkInstance;

class BackendWayland : public Backend
{
public:
    BackendWayland( VlkInstance& vkInstance, GpuDevices& gpus, GpuConnectors& connectors );
    ~BackendWayland() override;

    void Run( const std::function<void()>& render ) override;
    void Stop() override;

    void PointerMotion( double x, double y );

private:
    void RegistryGlobal( wl_registry* reg, uint32_t name, const char* interface, uint32_t version );
    void RegistryGlobalRemove( wl_registry* reg, uint32_t name );

    void XdgWmPing( xdg_wm_base* shell, uint32_t serial );

    void DecorationConfigure( zxdg_toplevel_decoration_v1* tldec, uint32_t mode );

    void OnOutput();

    wl_display* m_dpy;
    wl_compositor* m_compositor = nullptr;
    xdg_wm_base* m_xdgWmBase = nullptr;
    zxdg_decoration_manager_v1* m_decorationManager = nullptr;
    zxdg_toplevel_decoration_v1* m_toplevelDecoration = nullptr;

    std::unique_ptr<WaylandSeat> m_seat;
    std::unique_ptr<WaylandWindow> m_window;

    unordered_flat_map<uint32_t, std::unique_ptr<WaylandOutput>> m_outputMap;

    int32_t m_scale = 1;
    bool m_keepRunning = true;
};
