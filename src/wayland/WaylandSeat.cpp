#include "WaylandDisplay.hpp"
#include "WaylandKeyboard.hpp"
#include "WaylandPointer.hpp"
#include "WaylandSeat.hpp"
#include "util/Invoke.hpp"

WaylandSeat::WaylandSeat( wl_seat* seat, WaylandDisplay& dpy )
    : m_seat( seat )
    , m_dpy( dpy )
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
    if( m_dataDevice ) wl_data_device_destroy( m_dataDevice );
    wl_seat_destroy( m_seat );
}

void WaylandSeat::SetCursorShapeManager( wp_cursor_shape_manager_v1* cursorShapeManager )
{
    m_cursorShapeManager = cursorShapeManager;
    if( m_pointer ) m_pointer->SetCursorShapeManager( cursorShapeManager );
}

void WaylandSeat::SetDataDeviceManager( wl_data_device_manager* dataDeviceManager )
{
    static constexpr wl_data_device_listener listener = {
        .data_offer = Method( DataOffer ),
        .enter = Method( DataEnter ),
        .leave = Method( DataLeave ),
        .motion = Method( DataMotion ),
        .drop = Method( DataDrop ),
        .selection = Method( DataSelection )
    };

    m_dataDevice = wl_data_device_manager_get_data_device( dataDeviceManager, m_seat );
    wl_data_device_add_listener( m_dataDevice, &listener, this );
}

void WaylandSeat::Capabilities( wl_seat* seat, uint32_t caps )
{
    const bool hasPointer = caps & WL_SEAT_CAPABILITY_POINTER;
    const bool hasKeyboard = caps & WL_SEAT_CAPABILITY_KEYBOARD;

    if( hasPointer && !m_pointer )
    {
        m_pointer = std::make_unique<WaylandPointer>( wl_seat_get_pointer( seat ), *this );
        if( m_cursorShapeManager ) m_pointer->SetCursorShapeManager( m_cursorShapeManager );
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

void WaylandSeat::DataOffer( wl_data_device* dev, wl_data_offer* id )
{
}

void WaylandSeat::DataEnter( wl_data_device* dev, uint32_t serial, wl_surface* surf, wl_fixed_t x, wl_fixed_t y, wl_data_offer* id )
{
}

void WaylandSeat::DataLeave( wl_data_device* dev )
{
}

void WaylandSeat::DataMotion( wl_data_device* dev, uint32_t time, wl_fixed_t x, wl_fixed_t y )
{
}

void WaylandSeat::DataDrop( wl_data_device* dev )
{
}

void WaylandSeat::DataSelection( wl_data_device* dev, wl_data_offer* id )
{
}

void WaylandSeat::PointerMotion( double x, double y )
{
    //m_backend.PointerMotion( x, y );
}
