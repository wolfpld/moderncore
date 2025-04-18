#include "WaylandCursor.hpp"
#include "WaylandPointer.hpp"
#include "WaylandSeat.hpp"
#include "util/Invoke.hpp"
#include "util/Panic.hpp"

WaylandPointer::WaylandPointer( wl_pointer* pointer, WaylandSeat& seat )
    : m_pointer( pointer )
    , m_seat( seat )
    , m_scroll {}
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

void WaylandPointer::SetCursor( wl_surface* window, WaylandCursor cursor )
{
    if( !m_cursorShapeDevice ) return;
    if( m_activeWindow != window ) return;
    wp_cursor_shape_device_v1_set_shape( m_cursorShapeDevice, m_enterSerial, (wp_cursor_shape_device_v1_shape)cursor );
}

void WaylandPointer::Enter( wl_pointer* pointer, uint32_t serial, wl_surface* window, wl_fixed_t sx, wl_fixed_t sy )
{
    if( m_cursorShapeDevice )
    {
        auto& cursorMap = m_seat.m_cursorMap;
        auto it = cursorMap.find( window );
        CheckPanic( it != cursorMap.end(), "Unknown window entered!" );
        wp_cursor_shape_device_v1_set_shape( m_cursorShapeDevice, serial, (wp_cursor_shape_device_v1_shape)it->second );
    }

    m_enterSerial = serial;
    m_activeWindow = window;

    m_seat.PointerEntered( window, sx, sy );
}

void WaylandPointer::Leave( wl_pointer* pointer, uint32_t serial, wl_surface* window )
{
    CheckPanic( m_activeWindow == window, "Unknown window left!" );
    m_activeWindow = nullptr;
    m_seat.PointerLeft( window );
}

void WaylandPointer::Motion( wl_pointer* pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy )
{
    m_seat.PointerMotion( m_activeWindow, sx, sy );
}

void WaylandPointer::Button( wl_pointer* pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state )
{
    m_seat.PointerButton( m_activeWindow, button, state == WL_POINTER_BUTTON_STATE_PRESSED );
}

void WaylandPointer::Axis( wl_pointer* pointer, uint32_t time, uint32_t axis, wl_fixed_t value )
{
    if( axis == WL_POINTER_AXIS_VERTICAL_SCROLL )
    {
        m_scroll.delta.y += wl_fixed_to_double( value );
    }
    else
    {
        CheckPanic( axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL, "Unknown scroll axis!" );
        m_scroll.delta.x += wl_fixed_to_double( value );
    }
}

void WaylandPointer::Frame( wl_pointer* pointer )
{
    if( m_scroll.delta.x != 0 || m_scroll.delta.y != 0 )
    {
        m_seat.PointerScroll( m_activeWindow, m_scroll );
        m_scroll = {};
    }
}

void WaylandPointer::AxisSource( wl_pointer* pointer, uint32_t source )
{
    switch( source )
    {
    case WL_POINTER_AXIS_SOURCE_WHEEL:
        m_scroll.source = WaylandScroll::Source::Wheel;
        break;
    case WL_POINTER_AXIS_SOURCE_FINGER:
        m_scroll.source = WaylandScroll::Source::Finger;
        break;
    case WL_POINTER_AXIS_SOURCE_CONTINUOUS:
        m_scroll.source = WaylandScroll::Source::Continuous;
        break;
    case WL_POINTER_AXIS_SOURCE_WHEEL_TILT:
        m_scroll.source = WaylandScroll::Source::Tilt;
        break;
    default:
        CheckPanic( false, "Unknown scroll source!" );
    };
}

void WaylandPointer::AxisStop( wl_pointer* pointer, uint32_t time, uint32_t axis )
{
}

void WaylandPointer::AxisValue120( wl_pointer* pointer, uint32_t axis, int32_t value120 )
{
}

void WaylandPointer::AxisRelativeDirection( wl_pointer* pointer, uint32_t axis, uint32_t direction )
{
    CheckPanic( direction == WL_POINTER_AXIS_RELATIVE_DIRECTION_IDENTICAL ||
                direction == WL_POINTER_AXIS_RELATIVE_DIRECTION_INVERTED, "Unknown scroll direction!" );

    if( axis == WL_POINTER_AXIS_VERTICAL_SCROLL )
    {
        m_scroll.inverted.y = direction == WL_POINTER_AXIS_RELATIVE_DIRECTION_INVERTED;
    }
    else
    {
        CheckPanic( axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL, "Unknown scroll axis!" );
        m_scroll.inverted.x = direction == WL_POINTER_AXIS_RELATIVE_DIRECTION_INVERTED;
    }
}
