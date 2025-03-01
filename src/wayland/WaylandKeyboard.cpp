#include "WaylandKeyboard.hpp"
#include "util/Invoke.hpp"

WaylandKeyboard::WaylandKeyboard( wl_keyboard *keyboard )
    : m_keyboard( keyboard )
{
    static constexpr wl_keyboard_listener listener = {
        .keymap = Method( Keymap ),
        .enter = Method( Enter ),
        .leave = Method( Leave ),
        .key = Method( Key ),
        .modifiers = Method( Modifiers ),
        .repeat_info = Method( RepeatInfo )
    };

    wl_keyboard_add_listener( m_keyboard, &listener, this );
}

WaylandKeyboard::~WaylandKeyboard()
{
    wl_keyboard_destroy( m_keyboard );
}

void WaylandKeyboard::Keymap( wl_keyboard* kbd, uint32_t format, int32_t fd, uint32_t size )
{
}

void WaylandKeyboard::Enter( wl_keyboard* kbd, uint32_t serial, wl_surface* surf, wl_array* keys )
{
    m_activeWindow = surf;
}

void WaylandKeyboard::Leave( wl_keyboard* kbd, uint32_t serial, wl_surface* surf )
{
    m_activeWindow = nullptr;
}

void WaylandKeyboard::Key( wl_keyboard* kbd, uint32_t serial, uint32_t time, uint32_t key, uint32_t state )
{
}

void WaylandKeyboard::Modifiers( wl_keyboard* kbd, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group )
{
}

void WaylandKeyboard::RepeatInfo( wl_keyboard* kbd, int32_t rate, int32_t delay )
{
}
