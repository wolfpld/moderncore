#include "BackendWayland.hpp"
#include "WaylandKeyboard.hpp"
#include "WaylandMethod.hpp"
#include "WaylandPointer.hpp"
#include "WaylandSeat.hpp"

WaylandSeat::WaylandSeat( wl_seat* seat, BackendWayland& backend )
    : m_seat( seat )
    , m_backend( backend )
{
    static constexpr wl_seat_listener listener = {
        .capabilities = Method( Capabilities ),
        .name = Method( Name )
    };

    wl_seat_add_listener( m_seat, &listener, this );
}

WaylandSeat::~WaylandSeat()
{
    m_pointer.reset();
    m_keyboard.reset();
    wl_seat_destroy( m_seat );
}

void WaylandSeat::Capabilities( wl_seat* seat, uint32_t caps )
{
    const bool hasPointer = caps & WL_SEAT_CAPABILITY_POINTER;
    const bool hasKeyboard = caps & WL_SEAT_CAPABILITY_KEYBOARD;

    if( hasPointer && !m_pointer )
    {
        m_pointer = std::make_unique<WaylandPointer>( wl_seat_get_pointer( seat ), *this );
    }
    else if( !hasPointer && m_pointer )
    {
        m_pointer.reset();
    }

    if( hasKeyboard && !m_keyboard )
    {
        m_keyboard = std::make_unique<WaylandKeyboard>( wl_seat_get_keyboard( seat ) );
    }
    else if( !hasKeyboard && m_keyboard )
    {
        m_keyboard.reset();
    }
}

void WaylandSeat::Name( wl_seat* seat, const char* name )
{
}

void WaylandSeat::PointerMotion( double x, double y )
{
    m_backend.PointerMotion( x, y );
}
