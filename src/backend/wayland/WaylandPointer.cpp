#include "WaylandMethod.hpp"
#include "WaylandPointer.hpp"
#include "WaylandSeat.hpp"

WaylandPointer::WaylandPointer( wl_pointer* pointer, WaylandSeat& seat )
    : m_pointer( pointer )
    , m_seat( seat )
{
    static constexpr wl_pointer_listener listener = {
        .enter = Method( PointerEnter ),
        .leave = Method( PointerLeave ),
        .motion = Method( PointerMotion ),
        .button = Method( PointerButton ),
        .axis = Method( PointerAxis ),
        .frame = Method( PointerFrame ),
        .axis_source = Method( PointerAxisSource ),
        .axis_stop = Method( PointerAxisStop ),
        .axis_discrete = Method( PointerAxisDiscrete ),
        .axis_value120 = Method( PointerAxisValue120 ),
        .axis_relative_direction = Method( PointerAxisRelativeDirection )
    };

    wl_pointer_add_listener( m_pointer, &listener, this );
}

WaylandPointer::~WaylandPointer()
{
    wl_pointer_destroy( m_pointer );
}

void WaylandPointer::PointerEnter( wl_pointer* pointer, uint32_t serial, wl_surface* surf, wl_fixed_t sx, wl_fixed_t sy )
{
    wl_pointer_set_cursor( pointer, serial, nullptr, 0, 0 );
    m_seat.PointerMotion( wl_fixed_to_double( sx ), wl_fixed_to_double( sy ) );
}

void WaylandPointer::PointerLeave( wl_pointer* pointer, uint32_t serial, wl_surface* surf )
{
}

void WaylandPointer::PointerMotion( wl_pointer* pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy )
{
    m_seat.PointerMotion( wl_fixed_to_double( sx ), wl_fixed_to_double( sy ) );
}

void WaylandPointer::PointerButton( wl_pointer* pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state )
{
}

void WaylandPointer::PointerAxis( wl_pointer* pointer, uint32_t time, uint32_t axis, wl_fixed_t value )
{
}

void WaylandPointer::PointerFrame( wl_pointer* pointer )
{
}

void WaylandPointer::PointerAxisSource( wl_pointer* pointer, uint32_t source )
{
}

void WaylandPointer::PointerAxisStop( wl_pointer* pointer, uint32_t time, uint32_t axis )
{
}

void WaylandPointer::PointerAxisDiscrete( wl_pointer* pointer, uint32_t axis, int32_t discrete )
{
}

void WaylandPointer::PointerAxisValue120( wl_pointer* pointer, uint32_t axis, int32_t value120 )
{
}

void WaylandPointer::PointerAxisRelativeDirection( wl_pointer* pointer, uint32_t axis, uint32_t direction )
{
}
