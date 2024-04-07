#pragma once

#include <functional>
#include <memory>
#include <stdint.h>

#include "backend/Backend.hpp"
#include "util/NoCopy.hpp"

class WaylandDisplay;
class WaylandWindow;

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

private:
    void OpenWindow( int physDev, uint32_t width, uint32_t height );

    void Close( WaylandWindow* );
    void Render( WaylandWindow* );

    std::unique_ptr<WaylandDisplay> m_dpy;
    std::vector<std::unique_ptr<WaylandWindow>> m_windows;

    std::function<void()> m_render;
};
