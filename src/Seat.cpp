#include "Display.hpp"
#include "Panic.hpp"
#include "Seat.hpp"

extern "C"
{
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_seat.h>
};

Seat::Seat( const Display& dpy )
    : m_seat( wlr_seat_create( dpy, "seat0" ) )
{
    CheckPanic( m_seat, "Failed to create wlr_seat!" );
}

Seat::~Seat()
{
    wlr_seat_destroy( m_seat );
}
