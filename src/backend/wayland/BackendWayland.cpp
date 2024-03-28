#include <format>
#include <string.h>
#include <tracy/Tracy.hpp>

#include "BackendWayland.hpp"
#include "WaylandMethod.hpp"
#include "WaylandOutput.hpp"
#include "WaylandRegistry.hpp"
#include "WaylandSeat.hpp"
#include "WaylandWindow.hpp"
#include "server/Server.hpp"
#include "util/Config.hpp"
#include "util/Panic.hpp"

BackendWayland::BackendWayland()
{
    ZoneScoped;

    m_dpy = wl_display_connect( nullptr );
    CheckPanic( m_dpy, "Failed to connect to Wayland display" );

    static constexpr wl_registry_listener listener = {
        .global = Method( RegistryGlobal ),
        .global_remove = Method( RegistryGlobalRemove )
    };

    wl_registry_add_listener( wl_display_get_registry( m_dpy ), &listener, this );
    wl_display_roundtrip( m_dpy );

    CheckPanic( m_compositor, "Failed to create Wayland compositor" );
    CheckPanic( m_xdgWmBase, "Failed to create Wayland xdg_wm_base" );
    CheckPanic( m_seat, "Failed to create Wayland seat" );
}

BackendWayland::~BackendWayland()
{
    m_windows.clear();
    if( m_viewporter ) wp_viewporter_destroy( m_viewporter );
    if( m_fractionalScaleManager ) wp_fractional_scale_manager_v1_destroy( m_fractionalScaleManager );
    if( m_decorationManager ) zxdg_decoration_manager_v1_destroy( m_decorationManager );
    m_outputMap.clear();
    m_seat.reset();
    xdg_wm_base_destroy( m_xdgWmBase );
    wl_compositor_destroy( m_compositor );
    wl_display_disconnect( m_dpy );
}

void BackendWayland::VulkanInit()
{
    Config config( "backend-wayland.ini" );
    if( config )
    {
        bool windowAdded = false;
        int idx = 0;
        while( true )
        {
            const auto section = std::format( "Window{}", idx );
            int physDev;
            if( !config.GetOpt( section.c_str(), "PhysicalDevice", physDev ) ) break;
            OpenWindow( physDev );
            windowAdded = true;
            idx++;
        }
        CheckPanic( windowAdded, "No windows to open configured in backend-wayland.ini." );
    }
    else
    {
        OpenWindow();
    }
}

void BackendWayland::Run( const std::function<void()>& render )
{
    for( auto& window : m_windows ) window->Show( render );
    while( m_keepRunning && wl_display_dispatch( m_dpy ) != -1 ) {}
}

void BackendWayland::Stop()
{
    m_keepRunning = false;
}

void BackendWayland::RegistryGlobal( wl_registry* reg, uint32_t name, const char* interface, uint32_t version )
{
    if( strcmp( interface, wl_compositor_interface.name ) == 0 )
    {
        m_compositor = RegistryBind( wl_compositor, 3, 3 );
    }
    else if( strcmp( interface, xdg_wm_base_interface.name ) == 0 )
    {
        static constexpr xdg_wm_base_listener listener = {
            .ping = Method( XdgWmPing )
        };

        m_xdgWmBase = RegistryBind( xdg_wm_base );
        xdg_wm_base_add_listener( m_xdgWmBase, &listener, this );
    }
    else if( strcmp( interface, wl_seat_interface.name ) == 0 )
    {
        auto seat = RegistryBind( wl_seat, 5, 9 );
        m_seat = std::make_unique<WaylandSeat>( seat, *this );
    }
    else if( strcmp( interface, wl_output_interface.name ) == 0 )
    {
        auto output = RegistryBind( wl_output, 3, 4 );
        m_outputMap.emplace( name, std::make_unique<WaylandOutput>( output, name ) );
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
}

void BackendWayland::RegistryGlobalRemove( wl_registry* reg, uint32_t name )
{
    auto it = m_outputMap.find( name );
    if( it != m_outputMap.end() ) m_outputMap.erase( it );
}

void BackendWayland::XdgWmPing( xdg_wm_base* shell, uint32_t serial )
{
    xdg_wm_base_pong( shell, serial );
}

void BackendWayland::PointerMotion( double x, double y )
{
    //m_window->PointerMotion( x * m_scale, y * m_scale );
}

void BackendWayland::OpenWindow( int physDev )
{
    mclog( LogLevel::Info, "Opening window on physical device %i", physDev );

    m_windows.emplace_back( std::make_unique<WaylandWindow>( WaylandWindow::Params {
        .physDev = physDev,
        .compositor = m_compositor,
        .xdgWmBase = m_xdgWmBase,
        .decorationManager = m_decorationManager,
        .dpy = m_dpy,
        .onClose = [this]{ Stop(); },
        .backend = *this
    } ) );
}
