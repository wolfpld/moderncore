#include <assert.h>

#include "Backend.hpp"
#include "Display.hpp"
#include "Panic.hpp"
#include "Seat.hpp"

extern "C"
{
#define WLR_USE_UNSTABLE
#include <wlr/backend.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/log.h>
};

Seat::Seat( const Display& dpy, const Backend& backend )
    : m_seat( wlr_seat_create( dpy, "seat0" ) )
    , m_newInput( backend.Get()->events.new_input, [this](auto v){ NewInput( v ); } )
    , m_reqCursor( m_seat->events.request_set_cursor, [this](auto v){ ReqCursor( v ); } )
    , m_reqSetSelection( m_seat->events.request_set_selection, [this](auto v){ ReqSetSelection( v ); } )
{
    CheckPanic( m_seat, "Failed to create wlr_seat!" );
}

Seat::~Seat()
{
    wlr_seat_destroy( m_seat );
}

void Seat::NewInput( wlr_input_device* dev )
{
    const char* type;
    switch( dev->type )
    {
    case WLR_INPUT_DEVICE_KEYBOARD:
        type = "keyboard";
        break;
    case WLR_INPUT_DEVICE_POINTER:
        type = "pointer";
        break;
    case WLR_INPUT_DEVICE_TOUCH:
        type = "touch";
        break;
    case WLR_INPUT_DEVICE_TABLET_TOOL:
        type = "tablet tool";
        break;
    case WLR_INPUT_DEVICE_TABLET_PAD:
        type = "tablet pad";
        break;
    case WLR_INPUT_DEVICE_SWITCH:
        type = "switch";
        break;
    default:
        wlr_log( WLR_ERROR, "Unknown input device type! (%i)", dev->type );
        return;
    }

    wlr_log( WLR_DEBUG, "New input: %s (%s)", dev->name, type );
}

void Seat::ReqCursor( wlr_seat_pointer_request_set_cursor_event* ev )
{
}

void Seat::ReqSetSelection( wlr_seat_request_set_selection_event* ev )
{
}
