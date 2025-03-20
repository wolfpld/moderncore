#include <tracy/Tracy.hpp>
#include <unistd.h>

#include "WaylandCursor.hpp"
#include "WaylandDataOffer.hpp"
#include "WaylandDisplay.hpp"
#include "WaylandKeyboard.hpp"
#include "WaylandPointer.hpp"
#include "WaylandSeat.hpp"
#include "WaylandWindow.hpp"
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
    m_nextOffer.reset();
    m_selectionOffer.reset();
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
    const auto surface = window->Surface();
    CheckPanic( m_windows.find( surface ) == m_windows.end(), "Window already added!" );
    m_windows.emplace( surface, window );
    CheckPanic( m_cursorMap.find( surface ) == m_cursorMap.end(), "Window already added!" );
    m_cursorMap.emplace( surface, WaylandCursor::Default );
}

void WaylandSeat::RemoveWindow( WaylandWindow* window )
{
    const auto surface = window->Surface();
    CheckPanic( m_windows.find( surface ) != m_windows.end(), "Window not found!" );
    m_windows.erase( surface );
    CheckPanic( m_cursorMap.find( surface ) != m_cursorMap.end(), "Window not added!" );
    m_cursorMap.erase( surface );
}

WaylandCursor WaylandSeat::GetCursor( wl_surface* window )
{
    auto it = m_cursorMap.find( window );
    CheckPanic( it != m_cursorMap.end(), "Getting cursor for an unknown window!" );
    return it->second;
}

void WaylandSeat::SetCursor( wl_surface* window, WaylandCursor cursor )
{
    if( m_pointer ) m_pointer->SetCursor( window, cursor );

    auto it = m_cursorMap.find( window );
    CheckPanic( it != m_cursorMap.end(), "Setting cursor on an unknown window!" );
    it->second = cursor;
}

int WaylandSeat::GetClipboard( const char* mime )
{
    ZoneScoped;
    CheckPanic( m_selectionOffer, "No data offer!" );

    int fd[2];
    if( pipe( fd ) != 0 ) return -1;
    wl_data_offer_receive( *m_selectionOffer, mime, fd[1] );
    close( fd[1] );
    wl_display_roundtrip( m_dpy.Display() );
    return fd[0];
}

void WaylandSeat::AcceptDndMime( const char* mime )
{
    CheckPanic( m_dndOffer, "No drag and drop offer!" );
    mclog( LogLevel::Debug, "Drag and drop accept mime %s", mime ? mime : "none" );
    wl_data_offer_accept( *m_dndOffer, m_dndSerial, mime );
    if( mime )
    {
        m_dndMime = mime;
    }
    else
    {
        m_dndMime.clear();
    }
}

void WaylandSeat::FinishDnd( int fd )
{
    auto it = m_pendingDnd.find( fd );
    CheckPanic( it != m_pendingDnd.end(), "DnD not pending!" );

    close( it->first );
    wl_data_offer_finish( *it->second );
    m_pendingDnd.erase( it );
}

void WaylandSeat::KeyboardLeave( wl_surface* surf )
{
    if( m_selectionOffer )
    {
        m_selectionOffer.reset();
        GetWindow( surf )->InvokeClipboard( {} );
    }
}

void WaylandSeat::KeyEvent( wl_surface* surf, uint32_t key, int mods, bool pressed )
{
    GetWindow( surf )->InvokeKeyEvent( key, mods, pressed );
}

void WaylandSeat::CharacterEntered( wl_surface* surf, const char* character )
{
    GetWindow( surf )->InvokeCharacter( character );
}

void WaylandSeat::PointerEntered( wl_surface* surf, wl_fixed_t x, wl_fixed_t y )
{
    GetWindow( surf )->InvokeMouseEnter( wl_fixed_to_double( x ), wl_fixed_to_double( y ) );
}

void WaylandSeat::PointerLeft( wl_surface* surf )
{
    GetWindow( surf )->InvokeMouseLeave();
}

void WaylandSeat::PointerMotion( wl_surface* surf, wl_fixed_t x, wl_fixed_t y )
{
    GetWindow( surf )->InvokeMouseMove( wl_fixed_to_double( x ), wl_fixed_to_double( y ) );
}

void WaylandSeat::PointerButton( wl_surface* surf, uint32_t button, bool pressed )
{
    GetWindow( surf )->InvokeMouseButton( button, pressed );
}

void WaylandSeat::PointerScroll( wl_surface* surf, const WaylandScroll& scroll )
{
    GetWindow( surf )->InvokeScroll( scroll );
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
    m_nextOffer = std::make_unique<WaylandDataOffer>( offer );
}

void WaylandSeat::DataEnter( wl_data_device* dev, uint32_t serial, wl_surface* surf, wl_fixed_t x, wl_fixed_t y, wl_data_offer* offer )
{
    m_dndSerial = serial;
    m_dndSurface = surf;
    m_dndMime.clear();

    if( offer )
    {
        m_dndOffer = std::move( m_nextOffer );
        m_nextOffer.reset();

        wl_data_offer_set_actions( *m_dndOffer, WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY, WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY );

        mclog( LogLevel::Debug, "Drag and drop enter offer with %zu mime types", m_dndOffer->MimeTypes().size() );
        GetWindow( surf )->InvokeDrag( m_dndOffer->MimeTypes() );
    }
    else
    {
        mclog( LogLevel::Debug, "Drag and drop clear" );
        m_dndOffer.reset();
        GetWindow( surf )->InvokeDrag( {} );
    }
}

void WaylandSeat::DataLeave( wl_data_device* dev )
{
    mclog( LogLevel::Debug, "Drag and drop leave" );
    m_dndMime.clear();
    m_dndOffer.reset();
    GetWindow( m_dndSurface )->InvokeDrag( {} );
}

void WaylandSeat::DataMotion( wl_data_device* dev, uint32_t time, wl_fixed_t x, wl_fixed_t y )
{
}

void WaylandSeat::DataDrop( wl_data_device* dev )
{
    // Should not happen. Happens on KDE.
    // https://bugs.kde.org/show_bug.cgi?id=500962
    if( m_dndMime.empty() )
    {
        mclog( LogLevel::Error, "No mime type accepted for drop, but drop happened anyways!" );
        m_dndOffer.reset();
        return;
    }

    mclog( LogLevel::Debug, "Drag and drop drop" );
    CheckPanic( !m_dndMime.empty(), "No drag and drop mime!" );

    auto dndMime = std::move( m_dndMime );
    m_dndMime.clear();
    auto dndOffer = std::move( m_dndOffer );
    m_dndOffer.reset();

    int fd[2];
    if( pipe( fd ) != 0 ) return;
    wl_data_offer_receive( *dndOffer, dndMime.c_str(), fd[1] );
    close( fd[1] );
    wl_display_roundtrip( m_dpy.Display() );

    CheckPanic( !m_pendingDnd.contains( fd[0] ), "DnD already pending!" );
    m_pendingDnd.emplace( fd[0], std::move( dndOffer ) );

    GetWindow( m_dndSurface )->InvokeDrop( fd[0], dndMime.c_str() );
}

void WaylandSeat::DataSelection( wl_data_device* dev, wl_data_offer* offer )
{
    if( offer )
    {
        m_selectionOffer = std::move( m_nextOffer );
        m_nextOffer.reset();

        mclog( LogLevel::Debug, "Data selection offer with %zu mime types", m_selectionOffer->MimeTypes().size() );
        GetFocusedWindow()->InvokeClipboard( m_selectionOffer->MimeTypes() );
    }
    else
    {
        mclog( LogLevel::Debug, "Data selection clear" );
        m_selectionOffer.reset();
        GetFocusedWindow()->InvokeClipboard( {} );
    }
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
