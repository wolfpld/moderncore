#include <sys/mman.h>
#include <xkbcommon/xkbcommon-compose.h>

#include "WaylandKeyboard.hpp"
#include "WaylandKeys.hpp"
#include "WaylandSeat.hpp"
#include "util/Invoke.hpp"

WaylandKeyboard::WaylandKeyboard( wl_keyboard *keyboard, WaylandSeat &seat )
    : m_keyboard( keyboard )
    , m_seat( seat )
    , m_ctx( xkb_context_new( XKB_CONTEXT_NO_FLAGS ) )
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
    if( m_composeState ) xkb_compose_state_unref( m_composeState );
    if( m_composeTable ) xkb_compose_table_unref( m_composeTable );
    if( m_state ) xkb_state_unref( m_state );
    if( m_keymap ) xkb_keymap_unref( m_keymap );
    xkb_context_unref( m_ctx );
    wl_keyboard_destroy( m_keyboard );
}

void WaylandKeyboard::Keymap( wl_keyboard* kbd, uint32_t format, int32_t fd, uint32_t size )
{
    if( format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 )
    {
        close( fd );
        return;
    }

    auto map = (char*)mmap( nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0 );
    close( fd );
    if( map == MAP_FAILED ) return;

    if( m_keymap ) xkb_keymap_unref( m_keymap );
    m_keymap = xkb_keymap_new_from_string( m_ctx, map, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS );
    munmap( map, size );
    if( !m_keymap ) return;

    if( m_state ) xkb_state_unref( m_state );
    m_state = xkb_state_new( m_keymap );

    const char* locale = getenv( "LC_ALL" );
    if( !locale )
    {
        locale = getenv( "LC_CTYPE" );
        if( !locale )
        {
            locale = getenv( "LANG" );
            if( !locale ) locale = "C";
        }
    }

    if( m_composeTable ) xkb_compose_table_unref( m_composeTable );
    m_composeTable = xkb_compose_table_new_from_locale( m_ctx, locale, XKB_COMPOSE_COMPILE_NO_FLAGS );

    if( m_composeState ) xkb_compose_state_unref( m_composeState );
    m_composeState = xkb_compose_state_new( m_composeTable, XKB_COMPOSE_STATE_NO_FLAGS );

    m_ctrl = xkb_keymap_mod_get_index( m_keymap, XKB_MOD_NAME_CTRL );
    m_alt = xkb_keymap_mod_get_index( m_keymap, XKB_MOD_NAME_ALT );
    m_shift = xkb_keymap_mod_get_index( m_keymap, XKB_MOD_NAME_SHIFT );
    m_super = xkb_keymap_mod_get_index( m_keymap, XKB_MOD_NAME_LOGO );
}

void WaylandKeyboard::Enter( wl_keyboard* kbd, uint32_t serial, wl_surface* surf, wl_array* keys )
{
    CheckPanic( m_activeWindow == nullptr, "Window already entered!" );
    m_activeWindow = surf;
}

void WaylandKeyboard::Leave( wl_keyboard* kbd, uint32_t serial, wl_surface* surf )
{
    CheckPanic( m_activeWindow == surf, "Unknown window left!" );
    m_seat.KeyboardLeave( surf );
    m_activeWindow = nullptr;
}

void WaylandKeyboard::Key( wl_keyboard* kbd, uint32_t serial, uint32_t time, uint32_t key, uint32_t state )
{
    if( state == WL_KEYBOARD_KEY_STATE_PRESSED )
    {
        m_seat.KeyEvent( m_activeWindow, key, m_modState, true );
    }
    else if( state == WL_KEYBOARD_KEY_STATE_RELEASED )
    {
        m_seat.KeyEvent( m_activeWindow, key, m_modState, false );
        return;
    }

    const xkb_keysym_t* keysyms;
    if( xkb_state_key_get_syms( m_state, key + 8, &keysyms ) == 1 )
    {
        const auto sym = Compose( keysyms[0] );
        char txt[8];
        if( xkb_keysym_to_utf8( sym, txt, sizeof( txt ) ) > 0 )
        {
            m_seat.CharacterEntered( m_activeWindow, txt );
        }
    }
}

void WaylandKeyboard::Modifiers( wl_keyboard* kbd, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group )
{
    xkb_state_update_mask( m_state, mods_depressed, mods_latched, mods_locked, 0, 0, group );

    m_modState = 0;
    if( xkb_state_mod_index_is_active( m_state, m_ctrl, XKB_STATE_MODS_EFFECTIVE ) ) m_modState |= CtrlBit;
    if( xkb_state_mod_index_is_active( m_state, m_alt, XKB_STATE_MODS_EFFECTIVE ) ) m_modState |= AltBit;
    if( xkb_state_mod_index_is_active( m_state, m_shift, XKB_STATE_MODS_EFFECTIVE ) ) m_modState |= ShiftBit;
    if( xkb_state_mod_index_is_active( m_state, m_super, XKB_STATE_MODS_EFFECTIVE ) ) m_modState |= SuperBit;
}

void WaylandKeyboard::RepeatInfo( wl_keyboard* kbd, int32_t rate, int32_t delay )
{
}

xkb_keysym_t WaylandKeyboard::Compose( const xkb_keysym_t sym )
{
    if( sym == XKB_KEY_NoSymbol ) return sym;
    if( xkb_compose_state_feed( m_composeState, sym ) != XKB_COMPOSE_FEED_ACCEPTED ) return sym;
    switch( xkb_compose_state_get_status( m_composeState ) )
    {
    case XKB_COMPOSE_COMPOSED:
        return xkb_compose_state_get_one_sym( m_composeState );
    case XKB_COMPOSE_COMPOSING:
    case XKB_COMPOSE_CANCELLED:
        return XKB_KEY_NoSymbol;
    case XKB_COMPOSE_NOTHING:
        return sym;
    }
}
