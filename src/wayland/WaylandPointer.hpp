#pragma once

#include <wayland-client.h>

#include "WaylandScroll.hpp"
#include "util/NoCopy.hpp"

#include "wayland-cursor-shape-client-protocol.h"

class WaylandSeat;
enum class WaylandCursor;

class WaylandPointer
{
    friend class WaylandSeat;

public:
    explicit WaylandPointer( wl_pointer* pointer, WaylandSeat& seat );
    ~WaylandPointer();

    NoCopy( WaylandPointer );

    void SetCursorShapeManager( wp_cursor_shape_manager_v1* cursorShapeManager );
    void SetCursor( wl_surface* window, WaylandCursor cursor );

private:
    void Enter( wl_pointer* pointer, uint32_t serial, wl_surface* window, wl_fixed_t sx, wl_fixed_t sy );
    void Leave( wl_pointer* pointer, uint32_t serial, wl_surface* window );
    void Motion( wl_pointer* pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy );
    void Button( wl_pointer* pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state );
    void Axis( wl_pointer* pointer, uint32_t time, uint32_t axis, wl_fixed_t value );
    void Frame( wl_pointer* pointer );
    void AxisSource( wl_pointer* pointer, uint32_t source );
    void AxisStop( wl_pointer* pointer, uint32_t time, uint32_t axis );
    void AxisValue120( wl_pointer* pointer, uint32_t axis, int32_t value120 );
    void AxisRelativeDirection( wl_pointer* pointer, uint32_t axis, uint32_t direction );

    wl_pointer* m_pointer;
    WaylandSeat& m_seat;

    uint32_t m_enterSerial = 0;
    wl_surface* m_activeWindow = nullptr;

    wp_cursor_shape_manager_v1* m_cursorShapeManager = nullptr;
    wp_cursor_shape_device_v1* m_cursorShapeDevice = nullptr;

    WaylandScroll m_scroll;
};
