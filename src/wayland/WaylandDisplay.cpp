#include <string.h>
#include <tracy/Tracy.hpp>

#include "WaylandDisplay.hpp"
#include "WaylandRegistry.hpp"
#include "WaylandSeat.hpp"
#include "util/Invoke.hpp"
#include "util/Panic.hpp"

WaylandDisplay::WaylandDisplay()
{
    ZoneScoped;

    m_dpy = wl_display_connect( nullptr );
    CheckPanic( m_dpy, "Failed to connect to Wayland display" );

    static constexpr wl_registry_listener listener = {
        .global = Method( RegistryGlobalShim ),
        .global_remove = Method( RegistryGlobalRemove )
    };

    wl_registry_add_listener( wl_display_get_registry( m_dpy ), &listener, this );
    wl_display_roundtrip( m_dpy );

    CheckPanic( m_compositor, "Failed to create Wayland compositor" );
    CheckPanic( m_shm, "Failed to create Wayland shm" );
    CheckPanic( m_xdgWmBase, "Failed to create Wayland xdg_wm_base" );
    CheckPanic( m_seat, "Failed to create Wayland seat" );
    CheckPanic( m_viewporter, "Failed to create Wayland viewporter" );
    CheckPanic( m_fractionalScaleManager, "Failed to create Wayland fractional scale manager" );
    CheckPanic( m_dataDeviceManager, "Failed to create Wayland data device manager" );

    if( !m_cursorShapeManager ) mclog( LogLevel::Warning, "Unable to set mouse cursors. Switch to a different compositor." );
}

WaylandDisplay::~WaylandDisplay()
{
    m_seat.reset();
    if( m_activation ) xdg_activation_v1_destroy( m_activation );
    if( m_dataDeviceManager ) wl_data_device_manager_destroy( m_dataDeviceManager );
    if( m_iconManager ) xdg_toplevel_icon_manager_v1_destroy( m_iconManager );
    if( m_cursorShapeManager ) wp_cursor_shape_manager_v1_destroy( m_cursorShapeManager );
    if( m_viewporter ) wp_viewporter_destroy( m_viewporter );
    if( m_fractionalScaleManager ) wp_fractional_scale_manager_v1_destroy( m_fractionalScaleManager );
    if( m_decorationManager ) zxdg_decoration_manager_v1_destroy( m_decorationManager );
    if( m_xdgWmBase ) xdg_wm_base_destroy( m_xdgWmBase );
    if( m_shm ) wl_shm_destroy( m_shm );
    if( m_compositor ) wl_compositor_destroy( m_compositor );
    if( m_dpy ) wl_display_disconnect( m_dpy );
}

void WaylandDisplay::Roundtrip()
{
    wl_display_roundtrip( m_dpy );
}

void WaylandDisplay::Run()
{
    while( m_keepRunning && wl_display_dispatch( m_dpy ) != -1 ) {}
}

void WaylandDisplay::RegistryGlobalShim( wl_registry* reg, uint32_t name, const char* interface, uint32_t version )
{
    ZoneScoped;
    ZoneText( interface, strlen( interface ) );

    RegistryGlobal( reg, name, interface, version );
}

void WaylandDisplay::RegistryGlobal( wl_registry* reg, uint32_t name, const char* interface, uint32_t version )
{
    if( strcmp( interface, wl_compositor_interface.name ) == 0 )
    {
        m_compositor = RegistryBind( wl_compositor, 5, 5 );
    }
    else if( strcmp( interface, wl_shm_interface.name ) == 0 )
    {
        m_shm = RegistryBind( wl_shm );
    }
    else if( strcmp( interface, xdg_wm_base_interface.name ) == 0 )
    {
        static constexpr xdg_wm_base_listener listener = {
            .ping = Method( XdgWmPing )
        };

        m_xdgWmBase = RegistryBind( xdg_wm_base, 1, 4 );
        xdg_wm_base_add_listener( m_xdgWmBase, &listener, this );
    }
    else if( strcmp( interface, wl_seat_interface.name ) == 0 )
    {
        auto seat = RegistryBind( wl_seat, 5, 10 );
        m_seat = std::make_unique<WaylandSeat>( seat, *this );
        if( m_dataDeviceManager ) m_seat->SetDataDeviceManager( m_dataDeviceManager );
        if( m_cursorShapeManager ) m_seat->SetCursorShapeManager( m_cursorShapeManager );
    }
    else if( strcmp( interface, zxdg_decoration_manager_v1_interface.name ) == 0 )
    {
        m_decorationManager = RegistryBind( zxdg_decoration_manager_v1 );
    }
    else if( strcmp( interface, wp_fractional_scale_manager_v1_interface.name ) == 0 )
    {
        m_fractionalScaleManager = RegistryBind( wp_fractional_scale_manager_v1 );
    }
    else if( strcmp( interface, wp_viewporter_interface.name ) == 0 )
    {
        m_viewporter = RegistryBind( wp_viewporter );
    }
    else if( strcmp( interface, wp_cursor_shape_manager_v1_interface.name ) == 0 )
    {
        m_cursorShapeManager = RegistryBind( wp_cursor_shape_manager_v1 );
        if( m_seat ) m_seat->SetCursorShapeManager( m_cursorShapeManager );
    }
    else if( strcmp( interface, xdg_toplevel_icon_manager_v1_interface.name ) == 0 )
    {
        static constexpr xdg_toplevel_icon_manager_v1_listener listener = {
            .icon_size = Method( IconManagerSize ),
            .done = []( void*, xdg_toplevel_icon_manager_v1* ) {}
        };

        m_iconManager = RegistryBind( xdg_toplevel_icon_manager_v1 );
        xdg_toplevel_icon_manager_v1_add_listener( m_iconManager, &listener, this );
        wl_display_roundtrip( m_dpy );
    }
    else if( strcmp( interface, wl_data_device_manager_interface.name ) == 0 )
    {
        m_dataDeviceManager = RegistryBind( wl_data_device_manager, 3, 3 );
        if( m_seat ) m_seat->SetDataDeviceManager( m_dataDeviceManager );
    }
    else if( strcmp( interface, xdg_activation_v1_interface.name ) == 0 )
    {
        m_activation = RegistryBind( xdg_activation_v1 );
    }
}

void WaylandDisplay::RegistryGlobalRemove( wl_registry* reg, uint32_t name )
{
}

void WaylandDisplay::XdgWmPing( xdg_wm_base* shell, uint32_t serial )
{
    xdg_wm_base_pong( shell, serial );
}

void WaylandDisplay::IconManagerSize( xdg_toplevel_icon_manager_v1* manager, int32_t size )
{
    m_iconSizes.emplace_back( size );
}
