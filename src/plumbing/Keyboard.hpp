#ifndef __WLR_KEYBOARD_HPP__
#define __WLR_KEYBOARD_HPP__

#include "Listener.hpp"

extern "C"
{
    struct wlr_input_device;
    struct wlr_keyboard;
    struct wlr_keyboard_key_event;
}

class Seat;

class Keyboard
{
public:
    Keyboard( Seat& seat, wlr_input_device* dev );

private:
    void Destroy();
    void ModifierEvent();
    void KeyEvent( wlr_keyboard_key_event* ev );

    Seat& m_seat;
    wlr_keyboard* m_kbd;

    Listener<void> m_destroy;
    Listener<void> m_modifierEvent;
    Listener<wlr_keyboard_key_event> m_keyEvent;
};

#endif
