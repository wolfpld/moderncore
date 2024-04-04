#pragma once

#include <memory>
#include <stdint.h>
#include <vector>
#include <wayland-client.h>

#include "backend/Backend.hpp"
#include "util/NoCopy.hpp"
#include "util/RobinHood.hpp"
#include "wayland/WaylandDisplay.hpp"

class WaylandBackendWindow;
class WaylandOutput;
class WaylandWindow;

class BackendWayland : public Backend, public WaylandDisplay
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
    void RegistryGlobal( wl_registry* reg, uint32_t name, const char* interface, uint32_t version ) override;
    void RegistryGlobalRemove( wl_registry* reg, uint32_t name ) override;

    void XdgWmPing( xdg_wm_base* shell, uint32_t serial );

    void OpenWindow( int physDev = -1 );

    void Close( WaylandWindow* );

    std::vector<std::unique_ptr<WaylandBackendWindow>> m_windows;
    unordered_flat_map<uint32_t, std::unique_ptr<WaylandOutput>> m_outputMap;

    bool m_keepRunning = true;
};
