#include "WaylandMethod.hpp"
#include "WaylandPointer.hpp"

WaylandPointer::WaylandPointer( wl_pointer* pointer )
    : m_pointer( pointer )
{
    static constexpr wl_pointer_listener pointerListener = {
        .enter = Method( WaylandPointer, PointerEnter ),
        .leave = Method( WaylandPointer, PointerLeave ),
        .motion = Method( WaylandPointer, PointerMotion ),
        .button = Method( WaylandPointer, PointerButton ),
        .axis = Method( WaylandPointer, PointerAxis ),
        .frame = Method( WaylandPointer, PointerFrame ),
        .axis_source = Method( WaylandPointer, PointerAxisSource ),
        .axis_stop = Method( WaylandPointer, PointerAxisStop ),
        .axis_discrete = Method( WaylandPointer, PointerAxisDiscrete ),
        .axis_value120 = Method( WaylandPointer, PointerAxisValue120 ),
        .axis_relative_direction = Method( WaylandPointer, PointerAxisRelativeDirection )
    };

    wl_pointer_add_listener( m_pointer, &pointerListener, this );
}

WaylandPointer::~WaylandPointer()
{
    wl_pointer_destroy( m_pointer );
}

void WaylandPointer::PointerEnter( wl_pointer* pointer, uint32_t serial, wl_surface* surf, wl_fixed_t sx, wl_fixed_t sy )
{
    wl_pointer_set_cursor( pointer, serial, nullptr, 0, 0 );
}

void WaylandPointer::PointerLeave( wl_pointer* pointer, uint32_t serial, wl_surface* surf )
{
}

void WaylandPointer::PointerMotion( wl_pointer* pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy )
{
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
