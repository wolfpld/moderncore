#include "WaylandMethod.hpp"
#include "WaylandPointer.hpp"
#include "WaylandSeat.hpp"

WaylandPointer::WaylandPointer( wl_pointer* pointer, WaylandSeat& seat )
    : m_pointer( pointer )
    , m_seat( seat )
{
    static constexpr wl_pointer_listener listener = {
        .enter = Method( Enter ),
        .leave = Method( Leave ),
        .motion = Method( Motion ),
        .button = Method( Button ),
        .axis = Method( Axis ),
        .frame = Method( Frame ),
        .axis_source = Method( AxisSource ),
        .axis_stop = Method( AxisStop ),
        .axis_discrete = Method( AxisDiscrete ),
        .axis_value120 = Method( AxisValue120 ),
        .axis_relative_direction = Method( AxisRelativeDirection )
    };

    wl_pointer_add_listener( m_pointer, &listener, this );
}

WaylandPointer::~WaylandPointer()
{
    if( m_cursorShapeDevice ) wp_cursor_shape_device_v1_destroy( m_cursorShapeDevice );
    wl_pointer_destroy( m_pointer );
}

void WaylandPointer::SetCursorShapeManager( wp_cursor_shape_manager_v1* cursorShapeManager )
{
    if( m_cursorShapeManager ) return;

    m_cursorShapeManager = cursorShapeManager;
    m_cursorShapeDevice = wp_cursor_shape_manager_v1_get_pointer( m_cursorShapeManager, m_pointer );
}

void WaylandPointer::Enter( wl_pointer* pointer, uint32_t serial, wl_surface* surf, wl_fixed_t sx, wl_fixed_t sy )
{
    wl_pointer_set_cursor( pointer, serial, nullptr, 0, 0 );
    m_seat.PointerMotion( wl_fixed_to_double( sx ), wl_fixed_to_double( sy ) );
}

void WaylandPointer::Leave( wl_pointer* pointer, uint32_t serial, wl_surface* surf )
{
}

void WaylandPointer::Motion( wl_pointer* pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy )
{
    m_seat.PointerMotion( wl_fixed_to_double( sx ), wl_fixed_to_double( sy ) );
}

void WaylandPointer::Button( wl_pointer* pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state )
{
}

void WaylandPointer::Axis( wl_pointer* pointer, uint32_t time, uint32_t axis, wl_fixed_t value )
{
}

void WaylandPointer::Frame( wl_pointer* pointer )
{
}

void WaylandPointer::AxisSource( wl_pointer* pointer, uint32_t source )
{
}

void WaylandPointer::AxisStop( wl_pointer* pointer, uint32_t time, uint32_t axis )
{
}

void WaylandPointer::AxisDiscrete( wl_pointer* pointer, uint32_t axis, int32_t discrete )
{
}

void WaylandPointer::AxisValue120( wl_pointer* pointer, uint32_t axis, int32_t value120 )
{
}

void WaylandPointer::AxisRelativeDirection( wl_pointer* pointer, uint32_t axis, uint32_t direction )
{
}
