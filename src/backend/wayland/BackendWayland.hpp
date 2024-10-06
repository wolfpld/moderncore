#pragma once

#include <memory>
#include <stdint.h>
#include <vector>

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

    void Run() override;
    void Stop() override;

    void PointerMotion( double x, double y );

private:
    void OpenWindow( int physDev, uint32_t width, uint32_t height, const char* title );

    void Close( WaylandWindow* );
    void Render( WaylandWindow* );

    std::unique_ptr<WaylandDisplay> m_dpy;
    std::vector<std::unique_ptr<WaylandWindow>> m_windows;
};
