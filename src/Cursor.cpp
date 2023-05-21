#include "Cursor.hpp"
#include "Panic.hpp"

extern "C" {
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
};

Cursor::Cursor()
    : m_cursor( wlr_cursor_create() )
    , m_manager( wlr_xcursor_manager_create( nullptr, 24 ) )
    , m_motion( m_cursor->events.motion, [this](auto v){ Motion( v ); } )
    , m_motionAbsolute( m_cursor->events.motion_absolute, [this](auto v){ MotionAbsolute( v ); } )
    , m_button( m_cursor->events.button, [this](auto v){ Button( v ); } )
    , m_axis( m_cursor->events.axis, [this](auto v){ Axis( v ); } )
    , m_frame( m_cursor->events.frame, [this](auto v){ Frame(); } )
{
    CheckPanic( m_cursor, "Failed to create wlr_cursor!" );
    CheckPanic( m_manager, "Failed to create wlr_xcursor_manager!" );

    wlr_xcursor_manager_load( m_manager, 1 );
}

Cursor::~Cursor()
{
    wlr_xcursor_manager_destroy( m_manager );
    wlr_cursor_destroy( m_cursor );
}

void Cursor::Motion( wlr_pointer_motion_event* ev )
{
}

void Cursor::MotionAbsolute( wlr_pointer_motion_absolute_event* ev )
{
}

void Cursor::Button( wlr_pointer_button_event* ev )
{
}

void Cursor::Axis( wlr_pointer_axis_event* ev )
{
}

void Cursor::Frame()
{
}
