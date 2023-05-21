#ifndef __CURSOR_HPP__
#define __CURSOR_HPP__

#include <wayland-server-core.h>

extern "C" {
    struct wlr_cursor;
    struct wlr_xcursor_manager;
    struct wlr_pointer_motion_event;
    struct wlr_pointer_motion_absolute_event;
    struct wlr_pointer_button_event;
    struct wlr_pointer_axis_event;
};

class Cursor
{
public:
    Cursor();
    ~Cursor();

    void Motion( struct wlr_pointer_motion_event* ev );
    void MotionAbsolute( struct wlr_pointer_motion_absolute_event* ev );
    void Button( struct wlr_pointer_button_event* ev );
    void Axis( struct wlr_pointer_axis_event* ev );
    void Frame();

private:
    struct wlr_cursor* m_cursor;
    struct wlr_xcursor_manager* m_manager;

    struct wl_listener m_motion;
    struct wl_listener m_motionAbsolute;
    struct wl_listener m_button;
    struct wl_listener m_axis;
    struct wl_listener m_frame;
};

#endif
