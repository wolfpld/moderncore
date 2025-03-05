#include "WaylandCursor.hpp"
#include "WaylandPointer.hpp"
#include "WaylandSeat.hpp"
#include "util/Invoke.hpp"
#include "util/Panic.hpp"

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

WaylandCursor WaylandPointer::GetCursor( wl_surface* window )
{
    auto it = m_cursorMap.find( window );
    CheckPanic( it != m_cursorMap.end(), "Getting cursor for an unknown window!" );
    return it->second;
}

void WaylandPointer::SetCursor( wl_surface* window, WaylandCursor cursor )
{
    if( m_cursorShapeDevice && m_activeWindow == window ) wp_cursor_shape_device_v1_set_shape( m_cursorShapeDevice, m_enterSerial, (wp_cursor_shape_device_v1_shape)cursor );
    auto it = m_cursorMap.find( window );
    CheckPanic( it != m_cursorMap.end(), "Setting cursor on an unknown window!" );
    it->second = cursor;
}

void WaylandPointer::AddWindow( wl_surface* window )
{
    CheckPanic( m_cursorMap.find( window ) == m_cursorMap.end(), "Window already added!" );
    m_cursorMap.emplace( window, WaylandCursor::Default );
}

void WaylandPointer::RemoveWindow( wl_surface* window )
{
    CheckPanic( m_cursorMap.find( window ) != m_cursorMap.end(), "Window not added!" );
    m_cursorMap.erase( window );
}

void WaylandPointer::Enter( wl_pointer* pointer, uint32_t serial, wl_surface* window, wl_fixed_t sx, wl_fixed_t sy )
{
    if( m_cursorShapeDevice )
    {
        auto it = m_cursorMap.find( window );
        CheckPanic( it != m_cursorMap.end(), "Unknown window entered!" );
        wp_cursor_shape_device_v1_set_shape( m_cursorShapeDevice, serial, (wp_cursor_shape_device_v1_shape)it->second );
    }

    m_enterSerial = serial;
    m_activeWindow = window;
}

void WaylandPointer::Leave( wl_pointer* pointer, uint32_t serial, wl_surface* window )
{
    CheckPanic( m_activeWindow == window, "Unknown window left!" );
    m_activeWindow = nullptr;
}

void WaylandPointer::Motion( wl_pointer* pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy )
{
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
