#pragma once

#include <memory>
#include <stdint.h>

class Background;
class BusyIndicator;
class ImageProvider;
class VlkDevice;
class VlkInstance;
class WaylandDisplay;
class WaylandWindow;

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

    std::shared_ptr<WaylandWindow> m_window;
    std::shared_ptr<VlkDevice> m_device;

    std::shared_ptr<Background> m_background;
    std::shared_ptr<BusyIndicator> m_busyIndicator;

    std::shared_ptr<ImageProvider> m_provider;

    float m_scale = 1.f;
    uint64_t m_lastTime = 0;
    bool m_isBusy = false;
};
