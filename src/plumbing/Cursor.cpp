#include "Cursor.hpp"
#include "Output.hpp"
#include "Seat.hpp"
#include "../cursor/CursorTheme.hpp"
#include "../util/Panic.hpp"

extern "C" {
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_seat.h>
};

Cursor::Cursor( const Seat& seat, const Output& output, const CursorTheme& theme )
    : m_seat( seat )
    , m_theme( theme )
    , m_cursor( wlr_cursor_create() )
    , m_motion( m_cursor->events.motion, [this](auto v){ Motion( v ); } )
    , m_motionAbsolute( m_cursor->events.motion_absolute, [this](auto v){ MotionAbsolute( v ); } )
    , m_button( m_cursor->events.button, [this](auto v){ Button( v ); } )
    , m_axis( m_cursor->events.axis, [this](auto v){ Axis( v ); } )
    , m_frame( m_cursor->events.frame, [this](auto){ Frame(); } )
{
    CheckPanic( m_cursor, "Failed to create wlr_cursor!" );

    wlr_cursor_attach_output_layout( m_cursor, output.GetLayout() );
}

Cursor::~Cursor()
{
    wlr_cursor_destroy( m_cursor );
}

void Cursor::Motion( wlr_pointer_motion_event* ev )
{
    wlr_cursor_move( m_cursor, &ev->pointer->base, ev->delta_x, ev->delta_y );
    m_theme.Set( m_cursor );
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
