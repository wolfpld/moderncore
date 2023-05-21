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

static Seat* s_instance;

Seat::Seat( const Display& dpy, const Backend& backend )
    : m_seat( wlr_seat_create( dpy, "seat0" ) )
{
    CheckPanic( m_seat, "Failed to create wlr_seat!" );

    CheckPanic( !s_instance, "Creating a second instance of Seat!" );
    s_instance = this;

    m_newInput.notify = []( wl_listener*, void* data ){ s_instance->NewInput( (wlr_input_device*)data ); };
    m_reqCursor.notify = []( wl_listener*, void* data ){ s_instance->ReqCursor( (wlr_seat_pointer_request_set_cursor_event*)data ); };
    m_reqSetSelection.notify = []( wl_listener*, void* data ){ s_instance->ReqSetSelection( (wlr_seat_request_set_selection_event*)data ); };

    wl_signal_add( &backend.Get()->events.new_input, &m_newInput );
    wl_signal_add( &m_seat->events.request_set_cursor, &m_reqCursor );
    wl_signal_add( &m_seat->events.request_set_selection, &m_reqSetSelection );
}

Seat::~Seat()
{
    wlr_seat_destroy( m_seat );
    s_instance = nullptr;
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
