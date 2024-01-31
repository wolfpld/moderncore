#pragma once

#include <memory>
#include <stdint.h>
#include <vector>
#include <wayland-client.h>

#include "backend/Backend.hpp"
#include "util/RobinHood.hpp"

#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

class GpuState;
class WaylandOutput;
class WaylandSeat;
class WaylandWindow;
class VlkInstance;

class BackendWayland : public Backend
{
public:
    BackendWayland( VlkInstance& vkInstance, GpuState& gpuState );
    ~BackendWayland() override;

    void Run( const std::function<void()>& render ) override;
    void Stop() override;

    void PointerMotion( double x, double y );

    [[nodiscard]] const auto& OutputMap() const { return m_outputMap; }

private:
    void RegistryGlobal( wl_registry* reg, uint32_t name, const char* interface, uint32_t version );
    void RegistryGlobalRemove( wl_registry* reg, uint32_t name );

    void XdgWmPing( xdg_wm_base* shell, uint32_t serial );

    void OpenWindow();

    wl_display* m_dpy;
    wl_compositor* m_compositor = nullptr;
    xdg_wm_base* m_xdgWmBase = nullptr;
    zxdg_decoration_manager_v1* m_decorationManager = nullptr;

    std::unique_ptr<WaylandSeat> m_seat;
    std::vector<std::unique_ptr<WaylandWindow>> m_windows;

    unordered_flat_map<uint32_t, std::unique_ptr<WaylandOutput>> m_outputMap;

    VlkInstance& m_vkInstance;
    GpuState& m_gpuState;

    bool m_keepRunning = true;
};
