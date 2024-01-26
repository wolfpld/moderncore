#include "WaylandKeyboard.hpp"
#include "WaylandMethod.hpp"

WaylandKeyboard::WaylandKeyboard( wl_keyboard *keyboard )
    : m_keyboard( keyboard )
{
    static constexpr struct wl_keyboard_listener keyboardListener = {
        .keymap = Method( KeyboardKeymap ),
        .enter = Method( KeyboardEnter ),
        .leave = Method( KeyboardLeave ),
        .key = Method( KeyboardKey ),
        .modifiers = Method( KeyboardModifiers ),
        .repeat_info = Method( KeyboardRepeatInfo )
    };

    wl_keyboard_add_listener( m_keyboard, &keyboardListener, this );
}

WaylandKeyboard::~WaylandKeyboard()
{
    wl_keyboard_destroy( m_keyboard );
}

void WaylandKeyboard::KeyboardKeymap( wl_keyboard* kbd, uint32_t format, int32_t fd, uint32_t size )
{
}

void WaylandKeyboard::KeyboardEnter( wl_keyboard* kbd, uint32_t serial, wl_surface* surf, wl_array* keys )
{
}

void WaylandKeyboard::KeyboardLeave( wl_keyboard* kbd, uint32_t serial, wl_surface* surf )
{
}

void WaylandKeyboard::KeyboardKey( wl_keyboard* kbd, uint32_t serial, uint32_t time, uint32_t key, uint32_t state )
{
}

void WaylandKeyboard::KeyboardModifiers( wl_keyboard* kbd, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group )
{
}

void WaylandKeyboard::KeyboardRepeatInfo( wl_keyboard* kbd, int32_t rate, int32_t delay )
{
}
