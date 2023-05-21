#include "Cursor.hpp"
#include "Panic.hpp"

extern "C" {
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
};

static Cursor* s_instance;

Cursor::Cursor()
    : m_cursor( wlr_cursor_create() )
    , m_manager( wlr_xcursor_manager_create( nullptr, 24 ) )
{
    CheckPanic( m_cursor, "Failed to create wlr_cursor!" );
    CheckPanic( m_manager, "Failed to create wlr_xcursor_manager!" );

    CheckPanic( !s_instance, "Creating a second instance of Cursor!" );
    s_instance = this;

    wlr_xcursor_manager_load( m_manager, 1 );

    m_motion.notify = []( struct wl_listener*, void* data ){ s_instance->Motion( (struct wlr_pointer_motion_event*)data ); };
    m_motionAbsolute.notify = []( struct wl_listener*, void* data ){ s_instance->MotionAbsolute( (struct wlr_pointer_motion_absolute_event*)data ); };
    m_button.notify = []( struct wl_listener*, void* data ){ s_instance->Button( (struct wlr_pointer_button_event*)data ); };
    m_axis.notify = []( struct wl_listener*, void* data ){ s_instance->Axis( (struct wlr_pointer_axis_event*)data ); };
    m_frame.notify = []( struct wl_listener*, void* data ){ s_instance->Frame(); };

    wl_signal_add( &m_cursor->events.motion, &m_motion );
    wl_signal_add( &m_cursor->events.motion_absolute, &m_motionAbsolute );
    wl_signal_add( &m_cursor->events.button, &m_button );
    wl_signal_add( &m_cursor->events.axis, &m_axis );
    wl_signal_add( &m_cursor->events.frame, &m_frame );
}

Cursor::~Cursor()
{
    wl_list_remove( &m_motion.link );
    wl_list_remove( &m_motionAbsolute.link );
    wl_list_remove( &m_button.link );
    wl_list_remove( &m_axis.link );
    wl_list_remove( &m_frame.link );

    wlr_xcursor_manager_destroy( m_manager );
    wlr_cursor_destroy( m_cursor );
    s_instance = nullptr;
}

void Cursor::Motion( struct wlr_pointer_motion_event* ev )
{
}

void Cursor::MotionAbsolute( struct wlr_pointer_motion_absolute_event* ev )
{
}

void Cursor::Button( struct wlr_pointer_button_event* ev )
{
}

void Cursor::Axis( struct wlr_pointer_axis_event* ev )
{
}

void Cursor::Frame()
{
}
