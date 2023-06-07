#include <string.h>

#include "BackendWayland.hpp"
#include "WaylandMethod.hpp"
#include "WaylandOutput.hpp"
#include "WaylandSeat.hpp"
#include "WaylandWindow.hpp"
#include "../../util/Panic.hpp"

BackendWayland::BackendWayland()
    : m_dpy( wl_display_connect( nullptr ) )
{
    CheckPanic( m_dpy, "Failed to connect to Wayland display" );

    static constexpr wl_registry_listener registryListener = {
        .global = Method( BackendWayland, RegistryGlobal ),
        .global_remove = Method( BackendWayland, RegistryGlobalRemove )
    };

    wl_registry_add_listener( wl_display_get_registry( m_dpy ), &registryListener, this );
    wl_display_roundtrip( m_dpy );

    CheckPanic( m_compositor, "Failed to create Wayland compositor" );
    CheckPanic( m_xdgWmBase, "Failed to create Wayland xdg_wm_base" );
    CheckPanic( m_seat, "Failed to create Wayland seat" );

    m_window = std::make_unique<WaylandWindow>( m_compositor, m_xdgWmBase, m_decorationManager, [this]{ Stop(); } );
    m_window->Show();
}

BackendWayland::~BackendWayland()
{
    m_window.reset();
    if( m_toplevelDecoration ) zxdg_toplevel_decoration_v1_destroy( m_toplevelDecoration );
    if( m_decorationManager ) zxdg_decoration_manager_v1_destroy( m_decorationManager );
    m_outputMap.clear();
    m_seat.reset();
    xdg_wm_base_destroy( m_xdgWmBase );
    wl_compositor_destroy( m_compositor );
    wl_display_disconnect( m_dpy );
}

void BackendWayland::Run()
{
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
        m_compositor = (wl_compositor*)wl_registry_bind( reg, name, &wl_compositor_interface, 1 );
    }
    else if( strcmp( interface, xdg_wm_base_interface.name ) == 0 )
    {
        static constexpr xdg_wm_base_listener wmBaseListener = {
            .ping = Method( BackendWayland, XdgWmPing )
        };

        m_xdgWmBase = (xdg_wm_base*)wl_registry_bind( reg, name, &xdg_wm_base_interface, 1 );
        xdg_wm_base_add_listener( m_xdgWmBase, &wmBaseListener, this );
    }
    else if( strcmp( interface, wl_seat_interface.name ) == 0 )
    {
        auto seat = (wl_seat*)wl_registry_bind( reg, name, &wl_seat_interface, 8 );
        m_seat = std::make_unique<WaylandSeat>( seat );
    }
    else if( strcmp( interface, wl_output_interface.name ) == 0 )
    {
        auto output = (wl_output*)wl_registry_bind( reg, name, &wl_output_interface, 4 );
        m_outputMap.emplace( name, std::make_unique<WaylandOutput>( output, [this]{ OnOutput(); } ) );
    }
    else if( strcmp( interface, zxdg_decoration_manager_v1_interface.name ) == 0 )
    {
        m_decorationManager = (zxdg_decoration_manager_v1*)wl_registry_bind( reg, name, &zxdg_decoration_manager_v1_interface, 1 );
    }
    else if( strcmp( interface, zxdg_toplevel_decoration_v1_interface.name ) == 0 )
    {
        static constexpr zxdg_toplevel_decoration_v1_listener decorationListener = {
            .configure = Method( BackendWayland, DecorationConfigure ),
        };

        m_toplevelDecoration = (zxdg_toplevel_decoration_v1*)wl_registry_bind( reg, name, &zxdg_toplevel_decoration_v1_interface, 1 );
        zxdg_toplevel_decoration_v1_add_listener( m_toplevelDecoration, &decorationListener, this );
        zxdg_toplevel_decoration_v1_set_mode( m_toplevelDecoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE );
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

void BackendWayland::DecorationConfigure( zxdg_toplevel_decoration_v1* tldec, uint32_t mode )
{
}

void BackendWayland::OnOutput()
{
    int32_t scale = 1;
    for( auto& it : m_outputMap )
    {
        const auto outputScale = it.second->GetScale();
        if( outputScale > scale ) scale = outputScale;
    }
    m_scale = scale;
}
