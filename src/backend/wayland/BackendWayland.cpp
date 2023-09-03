#include <algorithm>
#include <string.h>

#include "BackendWayland.hpp"
#include "WaylandMethod.hpp"
#include "WaylandOutput.hpp"
#include "WaylandRegistry.hpp"
#include "WaylandSeat.hpp"
#include "WaylandWindow.hpp"
#include "../../util/Panic.hpp"
#include "../../vulkan/VlkInstance.hpp"

BackendWayland::BackendWayland( VlkInstance& vkInstance, GpuState& gpuState )
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

    m_windows.emplace_back( std::make_unique<WaylandWindow>( WaylandWindow::Params {
        .compositor = m_compositor,
        .xdgWmBase = m_xdgWmBase,
        .decorationManager = m_decorationManager,
        .dpy = m_dpy,
        .vkInstance = vkInstance,
        .gpuState = gpuState,
        .onClose = [this]{ Stop(); }
    } ) );

    m_windows.emplace_back( std::make_unique<WaylandWindow>( WaylandWindow::Params {
        .compositor = m_compositor,
        .xdgWmBase = m_xdgWmBase,
        .decorationManager = m_decorationManager,
        .dpy = m_dpy,
        .vkInstance = vkInstance,
        .gpuState = gpuState,
        .onClose = [this]{ Stop(); }
    } ) );
}

BackendWayland::~BackendWayland()
{
    m_windows.clear();
    if( m_toplevelDecoration ) zxdg_toplevel_decoration_v1_destroy( m_toplevelDecoration );
    if( m_decorationManager ) zxdg_decoration_manager_v1_destroy( m_decorationManager );
    m_outputMap.clear();
    m_seat.reset();
    xdg_wm_base_destroy( m_xdgWmBase );
    wl_compositor_destroy( m_compositor );
    wl_display_disconnect( m_dpy );
}

void BackendWayland::Run( const std::function<void()>& render )
{
    for( auto& window : m_windows )
    {
        if( m_scale != 1 ) window->SetScale( m_scale );
        window->Show( render );
    }
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
        static constexpr xdg_wm_base_listener wmBaseListener = {
            .ping = Method( BackendWayland, XdgWmPing )
        };

        m_xdgWmBase = RegistryBind( xdg_wm_base );
        xdg_wm_base_add_listener( m_xdgWmBase, &wmBaseListener, this );
    }
    else if( strcmp( interface, wl_seat_interface.name ) == 0 )
    {
        auto seat = RegistryBind( wl_seat, 5, 9 );
        m_seat = std::make_unique<WaylandSeat>( seat, *this );
    }
    else if( strcmp( interface, wl_output_interface.name ) == 0 )
    {
        auto output = RegistryBind( wl_output, 3, 4 );
        m_outputMap.emplace( name, std::make_unique<WaylandOutput>( output, [this]{ OnOutput(); } ) );
    }
    else if( strcmp( interface, zxdg_decoration_manager_v1_interface.name ) == 0 )
    {
        m_decorationManager = RegistryBind( zxdg_decoration_manager_v1 );
    }
    else if( strcmp( interface, zxdg_toplevel_decoration_v1_interface.name ) == 0 )
    {
        static constexpr zxdg_toplevel_decoration_v1_listener decorationListener = {
            .configure = Method( BackendWayland, DecorationConfigure ),
        };

        m_toplevelDecoration = RegistryBind( zxdg_toplevel_decoration_v1 );
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
    if( scale != m_scale )
    {
        //if( m_window ) m_window->SetScale( scale );
        m_scale = scale;
    }
}

void BackendWayland::PointerMotion( double x, double y )
{
    //m_window->PointerMotion( x * m_scale, y * m_scale );
}
