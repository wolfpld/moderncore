#include <xkbcommon/xkbcommon.h>

#include "Keyboard.hpp"
#include "Seat.hpp"

extern "C"
{
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_seat.h>
};

Keyboard::Keyboard( Seat& seat, wlr_input_device* dev )
    : m_seat( seat )
    , m_kbd( wlr_keyboard_from_input_device( dev ) )
    , m_destroy( dev->events.destroy, [this](auto){ Destroy(); } )
    , m_modifierEvent( m_kbd->events.modifiers, [this](auto){ ModifierEvent(); } )
    , m_keyEvent( m_kbd->events.key, [this](auto v){ KeyEvent( v ); } )
{
    auto ctx = xkb_context_new( XKB_CONTEXT_NO_FLAGS );
    auto keymap = xkb_keymap_new_from_names( ctx, nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS );
    wlr_keyboard_set_keymap( m_kbd, keymap );
    xkb_keymap_unref( keymap );
    xkb_context_unref( ctx );

    wlr_keyboard_set_repeat_info( m_kbd, 25, 600 );

    wlr_seat_set_keyboard( m_seat, m_kbd );
}

void Keyboard::Destroy()
{
    m_seat.Remove( this );
}

void Keyboard::ModifierEvent()
{
    wlr_seat_set_keyboard( m_seat, m_kbd );
    wlr_seat_keyboard_notify_modifiers( m_seat, &m_kbd->modifiers );
}

void Keyboard::KeyEvent( wlr_keyboard_key_event* ev )
{
    if( m_seat.ProcessKeyEvent( ev, m_kbd ) ) return;

    wlr_seat_set_keyboard( m_seat, m_kbd );
    wlr_seat_keyboard_notify_key( m_seat, ev->time_msec, ev->keycode, ev->state );
}
