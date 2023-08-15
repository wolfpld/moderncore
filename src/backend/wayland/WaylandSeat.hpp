#pragma once

#include <memory>
#include <wayland-client.h>

#include "../../util/NoCopy.hpp"

class BackendWayland;
class WaylandKeyboard;
class WaylandPointer;

class WaylandSeat
{
public:
    explicit WaylandSeat( wl_seat* seat, BackendWayland& backend );
    ~WaylandSeat();

    NoCopy( WaylandSeat );

    void PointerMotion( double x, double y );

private:
    void SeatCapabilities( wl_seat* seat, uint32_t caps );
    void SeatName( wl_seat* seat, const char* name );

    wl_seat* m_seat;
    std::unique_ptr<WaylandPointer> m_pointer;
    std::unique_ptr<WaylandKeyboard> m_keyboard;

    BackendWayland& m_backend;
};
