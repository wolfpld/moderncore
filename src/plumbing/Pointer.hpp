#ifndef __WLR_POINTER_HPP__
#define __WLR_POINTER_HPP__

#include "Listener.hpp"

extern "C"
{
    struct wlr_input_device;
}

class Seat;

class Pointer
{
public:
    Pointer( Seat& seat, wlr_input_device* dev );
    ~Pointer();

private:
    void Destroy();

    Seat& m_seat;
    wlr_input_device* m_dev;

    Listener<wlr_input_device> m_destroy;
};

#endif
