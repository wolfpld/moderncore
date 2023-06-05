#include <assert.h>
#include <xkbcommon/xkbcommon.h>

#include "Backend.hpp"
#include "Display.hpp"
#include "Keyboard.hpp"
#include "Pointer.hpp"
#include "Seat.hpp"
#include "../util/Logs.hpp"
#include "../util/Panic.hpp"

extern "C"
{
#define WLR_USE_UNSTABLE
#include <wlr/backend.h>
#include <wlr/types/wlr_seat.h>
};

Seat::Seat( Display& dpy, const Backend& backend, const Output& output )
    : m_cursor( *this, output, m_theme )
    , m_dpy( dpy )
    , m_seat( wlr_seat_create( dpy, "seat0" ) )
    , m_newInput( backend.Get()->events.new_input, [this](auto v){ NewInput( v ); } )
    , m_reqCursor( m_seat->events.request_set_cursor, [this](auto v){ ReqCursor( v ); } )
    , m_reqSetSelection( m_seat->events.request_set_selection, [this](auto v){ ReqSetSelection( v ); } )
{
    CheckPanic( m_seat, "Failed to create wlr_seat!" );
}

Seat::~Seat()
{
    wlr_seat_destroy( m_seat );
}

void Seat::NewInput( wlr_input_device* dev )
{
    const char* type;
    switch( dev->type )
    {
    case WLR_INPUT_DEVICE_KEYBOARD:
        type = "keyboard";
        NewKeyboard( dev );
        break;
    case WLR_INPUT_DEVICE_POINTER:
        type = "pointer";
        NewPointer( dev );
        break;
    case WLR_INPUT_DEVICE_TOUCH:
        type = "touch";
        break;
    case WLR_INPUT_DEVICE_TABLET_TOOL:
        type = "tablet tool";
        break;
    case WLR_INPUT_DEVICE_TABLET_PAD:
        type = "tablet pad";
        break;
    case WLR_INPUT_DEVICE_SWITCH:
        type = "switch";
        break;
    default:
        mclog( LogLevel::Error, "Unknown input device type! (%i)", dev->type );
        return;
    }

    mclog( LogLevel::Info, "New input: %s (%s)", dev->name, type );
    UpdateCapabilities();
}

void Seat::ReqCursor( wlr_seat_pointer_request_set_cursor_event* ev )
{
}

void Seat::ReqSetSelection( wlr_seat_request_set_selection_event* ev )
{
}

void Seat::NewPointer( wlr_input_device* dev )
{
    m_pointers.emplace_back( std::make_unique<Pointer>( *this, dev ) );
}

void Seat::NewKeyboard( wlr_input_device* dev )
{
    m_keyboards.emplace_back( std::make_unique<Keyboard>( *this, dev ) );
}

void Seat::Remove( const Pointer* pointer )
{
    auto it = std::find_if( m_pointers.begin(), m_pointers.end(), [pointer]( const auto& v ) { return v.get() == pointer; } );
    assert( it != m_pointers.end() );
    m_pointers.erase( it );
    UpdateCapabilities();
}

void Seat::Remove( const Keyboard* keyboard )
{
    auto it = std::find_if( m_keyboards.begin(), m_keyboards.end(), [keyboard]( const auto& v ) { return v.get() == keyboard; } );
    assert( it != m_keyboards.end() );
    m_keyboards.erase( it );
    UpdateCapabilities();
}

bool Seat::ProcessKeyEvent( wlr_keyboard_key_event* ev, wlr_keyboard* kbd )
{
    if( ev->state == WL_KEYBOARD_KEY_STATE_PRESSED && ( wlr_keyboard_get_modifiers( kbd ) & WLR_MODIFIER_ALT ) )
    {
        const uint32_t keycode = ev->keycode + 8;
        const xkb_keysym_t* syms;
        const auto nsym = xkb_state_key_get_syms( kbd->xkb_state, keycode, &syms );

        for( int i=0; i<nsym; i++ )
        {
            if( syms[i] == XKB_KEY_Escape )
            {
                m_dpy.Terminate();
                return true;
            }
        }
    }
    return false;
}

void Seat::UpdateCapabilities()
{
    int caps = 0;
    if( !m_pointers.empty() ) caps |= WL_SEAT_CAPABILITY_POINTER;
    if( !m_keyboards.empty() ) caps |= WL_SEAT_CAPABILITY_KEYBOARD;
    wlr_seat_set_capabilities( m_seat, caps );
}
