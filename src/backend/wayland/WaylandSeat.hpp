#ifndef __WAYLANDSEAT_HPP__
#define __WAYLANDSEAT_HPP__

#include <memory>
#include <wayland-client.h>

class WaylandKeyboard;
class WaylandPointer;

class WaylandSeat
{
public:
    explicit WaylandSeat( wl_seat* seat );
    ~WaylandSeat();

private:
    void SeatCapabilities( wl_seat* seat, uint32_t caps );
    void SeatName( wl_seat* seat, const char* name );

    wl_seat* m_seat;
    std::unique_ptr<WaylandPointer> m_pointer;
    std::unique_ptr<WaylandKeyboard> m_keyboard;
};

#endif
