#pragma once

#include <memory>
#include <wayland-client.h>

#include "util/NoCopy.hpp"

#include "cursor-shape-v1-client-protocol.h"

class WaylandDisplay;
class WaylandKeyboard;
class WaylandPointer;

class WaylandSeat
{
public:
    explicit WaylandSeat( wl_seat* seat, WaylandDisplay& dpy );
    ~WaylandSeat();

    NoCopy( WaylandSeat );

    void PointerMotion( double x, double y );
    void SetCursorShapeManager( wp_cursor_shape_manager_v1* cursorShapeManager );

private:
    void Capabilities( wl_seat* seat, uint32_t caps );
    void Name( wl_seat* seat, const char* name );

    wl_seat* m_seat;
    std::unique_ptr<WaylandPointer> m_pointer;
    std::unique_ptr<WaylandKeyboard> m_keyboard;

    wp_cursor_shape_manager_v1* m_cursorShapeManager = nullptr;

    WaylandDisplay& m_dpy;
};
