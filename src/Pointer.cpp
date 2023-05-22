#include "Cursor.hpp"
#include "Pointer.hpp"
#include "Seat.hpp"

extern "C"
{
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_input_device.h>
};

Pointer::Pointer( Seat& seat, wlr_input_device* dev )
    : m_seat( seat )
    , m_dev( dev )
    , m_destroy( dev->events.destroy, [this](auto v){ Destroy(); } )
{
    wlr_cursor_attach_input_device( seat.GetCursor(), dev );
}

Pointer::~Pointer()
{
    wlr_cursor_detach_input_device( m_seat.GetCursor(), m_dev );
}

void Pointer::Destroy()
{
    m_seat.Remove( this );
}
