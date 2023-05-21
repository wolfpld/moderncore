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

private:
    void Motion( wlr_pointer_motion_event* ev );
    void MotionAbsolute( wlr_pointer_motion_absolute_event* ev );
    void Button( wlr_pointer_button_event* ev );
    void Axis( wlr_pointer_axis_event* ev );
    void Frame();

    wlr_cursor* m_cursor;
    wlr_xcursor_manager* m_manager;

    wl_listener m_motion;
    wl_listener m_motionAbsolute;
    wl_listener m_button;
    wl_listener m_axis;
    wl_listener m_frame;
};

#endif
