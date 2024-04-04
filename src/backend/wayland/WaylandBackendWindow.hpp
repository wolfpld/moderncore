#pragma once

#include <functional>
#include <memory>
#include <vulkan/vulkan.h>

#include "util/NoCopy.hpp"
#include "util/RobinHood.hpp"
#include "wayland/WaylandWindow.hpp"

class BackendWayland;
class CursorLogic;
class GpuDevice;
class SoftwareCursor;
class WaylandConnector;
class WaylandDisplay;
class VlkInstance;

class WaylandBackendWindow : public WaylandWindow
{
public:
    struct Params
    {
        int physDev;
        BackendWayland& backend;
    };

    WaylandBackendWindow( WaylandDisplay& display, VlkInstance& vkInstance, Params&& p );
    ~WaylandBackendWindow() override;

    NoCopy( WaylandBackendWindow );

    void Show( const std::function<void()>& render );
    void RenderCursor( VkCommandBuffer cmdBuf, CursorLogic& cursorLogic );

    void PointerMotion( double x, double y );

private:
    void Enter( struct wl_surface* surface, struct wl_output* output );
    void Leave( struct wl_surface* surface, struct wl_output* output );

    void FrameDone( struct wl_callback* cb, uint32_t time );

    std::function<void()> m_onRender;

    BackendWayland& m_backend;
    std::shared_ptr<GpuDevice> m_gpu;
    std::shared_ptr<WaylandConnector> m_connector;

    std::unique_ptr<SoftwareCursor> m_cursor;

    unordered_flat_set<uint32_t> m_outputs;
    int m_scale = 0;
    int m_id;
};
