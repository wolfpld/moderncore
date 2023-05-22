#include "Cursor.hpp"
#include "Output.hpp"
#include "Panic.hpp"
#include "Seat.hpp"

extern "C" {
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xcursor_manager.h>
};

Cursor::Cursor( const Seat& seat, const Output& output )
    : m_seat( seat )
    , m_cursor( wlr_cursor_create() )
    , m_manager( wlr_xcursor_manager_create( nullptr, 24 ) )
    , m_motion( m_cursor->events.motion, [this](auto v){ Motion( v ); } )
    , m_motionAbsolute( m_cursor->events.motion_absolute, [this](auto v){ MotionAbsolute( v ); } )
    , m_button( m_cursor->events.button, [this](auto v){ Button( v ); } )
    , m_axis( m_cursor->events.axis, [this](auto v){ Axis( v ); } )
    , m_frame( m_cursor->events.frame, [this](auto v){ Frame(); } )
{
    CheckPanic( m_cursor, "Failed to create wlr_cursor!" );
    CheckPanic( m_manager, "Failed to create wlr_xcursor_manager!" );

    wlr_cursor_attach_output_layout( m_cursor, output.GetLayout() );

    wlr_xcursor_manager_load( m_manager, 1 );
}

Cursor::~Cursor()
{
    wlr_xcursor_manager_destroy( m_manager );
    wlr_cursor_destroy( m_cursor );
}

void Cursor::Motion( wlr_pointer_motion_event* ev )
{
    wlr_cursor_move( m_cursor, &ev->pointer->base, ev->delta_x, ev->delta_y );
}

void Cursor::MotionAbsolute( wlr_pointer_motion_absolute_event* ev )
{
    wlr_cursor_warp_absolute( m_cursor, &ev->pointer->base, ev->x, ev->y );
}

void Cursor::Button( wlr_pointer_button_event* ev )
{
    wlr_seat_pointer_notify_button( m_seat, ev->time_msec, ev->button, ev->state );
}

void Cursor::Axis( wlr_pointer_axis_event* ev )
{
    wlr_seat_pointer_notify_axis( m_seat, ev->time_msec, ev->orientation, ev->delta, ev->delta_discrete, ev->source );
}

void Cursor::Frame()
{
    wlr_seat_pointer_notify_frame( m_seat );
}
