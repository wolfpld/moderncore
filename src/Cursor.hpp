#ifndef __CURSOR_HPP__
#define __CURSOR_HPP__

#include "Listener.hpp"

extern "C" {
    struct wlr_cursor;
    struct wlr_xcursor_manager;
    struct wlr_pointer_motion_event;
    struct wlr_pointer_motion_absolute_event;
    struct wlr_pointer_button_event;
    struct wlr_pointer_axis_event;
};

class Seat;

class Cursor
{
public:
    explicit Cursor( const Seat& seat );
    ~Cursor();

    [[nodiscard]] operator wlr_cursor* () const { return m_cursor; }

private:
    void Motion( wlr_pointer_motion_event* ev );
    void MotionAbsolute( wlr_pointer_motion_absolute_event* ev );
    void Button( wlr_pointer_button_event* ev );
    void Axis( wlr_pointer_axis_event* ev );
    void Frame();

    const Seat& m_seat;

    wlr_cursor* m_cursor;
    wlr_xcursor_manager* m_manager;

    Listener<wlr_pointer_motion_event> m_motion;
    Listener<wlr_pointer_motion_absolute_event> m_motionAbsolute;
    Listener<wlr_pointer_button_event> m_button;
    Listener<wlr_pointer_axis_event> m_axis;
    Listener<void> m_frame;
};

#endif
