#pragma once

#include <memory>
#include <stdint.h>

#include "wayland/WaylandWindow.hpp"

class Background;
class BusyIndicator;
class VlkDevice;
class VlkInstance;
class WaylandDisplay;

class Viewport
{
public:
    Viewport( WaylandDisplay& display, VlkInstance& vkInstance, int gpu );

private:
    void Close( WaylandWindow* window );
    bool Render( WaylandWindow* window );
    void Scale( WaylandWindow* window, uint32_t scale );
    void Resize( WaylandWindow* window, uint32_t width, uint32_t height );

    WaylandDisplay& m_display;
    VlkInstance& m_vkInstance;

    std::unique_ptr<WaylandWindow> m_window;
    std::shared_ptr<VlkDevice> m_device;

    std::shared_ptr<Background> m_background;
    std::shared_ptr<BusyIndicator> m_busyIndicator;

    float m_scale = 1.f;
};
