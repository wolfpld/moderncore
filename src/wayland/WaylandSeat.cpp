#include <tracy/Tracy.hpp>
#include <unistd.h>

#include "WaylandDisplay.hpp"
#include "WaylandKeyboard.hpp"
#include "WaylandPointer.hpp"
#include "WaylandSeat.hpp"
#include "WaylandWindow.hpp"
#include "util/Invoke.hpp"
#include "util/MemoryBuffer.hpp"

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
    if( m_dataOffer ) wl_data_offer_destroy( m_dataOffer );
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

void WaylandSeat::AddWindow( WaylandWindow* window )
{
    CheckPanic( m_windows.find( window->Surface() ) == m_windows.end(), "Window already added!" );
    m_windows.emplace( window->Surface(), window );
    m_pointer->AddWindow( window->Surface() );
}

void WaylandSeat::RemoveWindow( WaylandWindow* window )
{
    CheckPanic( m_windows.find( window->Surface() ) != m_windows.end(), "Window not found!" );
    m_windows.erase( window->Surface() );
    m_pointer->RemoveWindow( window->Surface() );
}

std::unique_ptr<MemoryBuffer> WaylandSeat::GetClipboard( const char* mime )
{
    ZoneScoped;

    std::vector<char> ret;

    CheckPanic( m_dataOffer, "No data offer!" );

    int fd[2];
    if( pipe( fd ) != 0 ) return std::make_unique<MemoryBuffer>( std::move( ret ) );
    wl_data_offer_receive( m_dataOffer, mime, fd[1] );
    close( fd[1] );
    wl_display_roundtrip( m_dpy.Display() );

    char buf[64*1024];
    while( true )
    {
        auto len = read( fd[0], buf, sizeof( buf ) );
        if( len <= 0 ) break;
        ret.insert( ret.end(), buf, buf + len );
    }

    close( fd[0] );
    return std::make_unique<MemoryBuffer>( std::move( ret ) );
}

void WaylandSeat::KeyboardLeave( wl_surface* surf )
{
    if( !m_dataOffer ) return;

    wl_data_offer_destroy( m_dataOffer );
    m_dataOffer = nullptr;
    m_offerMimeTypes.clear();

    GetWindow( surf )->InvokeClipboard( m_offerMimeTypes );
}

void WaylandSeat::KeyEntered( wl_surface* surf, const char* key, int mods )
{
    GetWindow( surf )->InvokeKey( key, mods );
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
        m_keyboard = std::make_unique<WaylandKeyboard>( wl_seat_get_keyboard( seat ), *this );
    }
    else if( !hasKeyboard && m_keyboard )
    {
        m_keyboard.reset();
    }
}

void WaylandSeat::Name( wl_seat* seat, const char* name )
{
}

void WaylandSeat::DataOffer( wl_data_device* dev, wl_data_offer* offer )
{
    if( m_dataOffer ) wl_data_offer_destroy( m_dataOffer );
    m_offerMimeTypes.clear();
    m_dataOffer = offer;

    static constexpr wl_data_offer_listener listener = {
        .offer = Method( DataOfferOffer ),
        .source_actions = Method( DataOfferSourceActions ),
        .action = Method( DataOfferAction )
    };

    wl_data_offer_add_listener( offer, &listener, this );
}

void WaylandSeat::DataEnter( wl_data_device* dev, uint32_t serial, wl_surface* surf, wl_fixed_t x, wl_fixed_t y, wl_data_offer* offer )
{
}

void WaylandSeat::DataLeave( wl_data_device* dev )
{
    if( m_dataOffer )
    {
        wl_data_offer_destroy( m_dataOffer );
        m_dataOffer = nullptr;
    }
}

void WaylandSeat::DataMotion( wl_data_device* dev, uint32_t time, wl_fixed_t x, wl_fixed_t y )
{
}

void WaylandSeat::DataDrop( wl_data_device* dev )
{
}

void WaylandSeat::DataSelection( wl_data_device* dev, wl_data_offer* offer )
{
    if( !offer && m_dataOffer )
    {
        wl_data_offer_destroy( m_dataOffer );
        m_dataOffer = nullptr;
        m_offerMimeTypes.clear();
    }
    GetFocusedWindow()->InvokeClipboard( m_offerMimeTypes );
}

void WaylandSeat::DataOfferOffer( wl_data_offer* offer, const char* mimeType )
{
    m_offerMimeTypes.emplace( mimeType );
}

void WaylandSeat::DataOfferSourceActions( wl_data_offer* offer, uint32_t sourceActions )
{
}

void WaylandSeat::DataOfferAction( wl_data_offer* offer, uint32_t dndAction )
{
}

WaylandWindow* WaylandSeat::GetFocusedWindow() const
{
    auto kbdFocus = m_keyboard->ActiveWindow();
    CheckPanic( kbdFocus, "No keyboard focus!" );
    return GetWindow( kbdFocus );
}

WaylandWindow* WaylandSeat::GetWindow( wl_surface* surf ) const
{
    auto it = m_windows.find( surf );
    CheckPanic( it != m_windows.end(), "Window not found!" );
    return it->second;
}
