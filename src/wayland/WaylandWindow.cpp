#include <tracy/Tracy.hpp>

#include "WaylandMethod.hpp"
#include "WaylandWindow.hpp"
#include "util/Panic.hpp"

WaylandWindow::WaylandWindow( const Params& p )
{
    ZoneScoped;

    m_surface = wl_compositor_create_surface( p.compositor );
    CheckPanic( m_surface, "Failed to create Wayland surface" );

    if( p.fractionalScaleManager && p.viewporter )
    {
        static constexpr wp_fractional_scale_v1_listener listener = {
            .preferred_scale = Method( FractionalScalePreferredScale )
        };

        m_fractionalScale = wp_fractional_scale_manager_v1_get_fractional_scale( p.fractionalScaleManager, m_surface );
        CheckPanic( m_fractionalScale, "Failed to create Wayland fractional scale" );
        wp_fractional_scale_v1_add_listener( m_fractionalScale, &listener, this );

        m_viewport = wp_viewporter_get_viewport( p.viewporter, m_surface );
        CheckPanic( m_viewport, "Failed to create Wayland viewport" );
    }

    static constexpr xdg_surface_listener xdgSurfaceListener = {
        .configure = Method( XdgSurfaceConfigure )
    };

    m_xdgSurface = xdg_wm_base_get_xdg_surface( p.xdgWmBase, m_surface );
    CheckPanic( m_xdgSurface, "Failed to create Wayland xdg_surface" );
    xdg_surface_add_listener( m_xdgSurface, &xdgSurfaceListener, this );

    static constexpr xdg_toplevel_listener toplevelListener = {
        .configure = Method( XdgToplevelConfigure ),
        .close = Method( XdgToplevelClose )
    };

    m_xdgToplevel = xdg_surface_get_toplevel( m_xdgSurface );
    CheckPanic( m_xdgToplevel, "Failed to create Wayland xdg_toplevel" );
    xdg_toplevel_add_listener( m_xdgToplevel, &toplevelListener, this );

    if( p.decorationManager )
    {
        static constexpr zxdg_toplevel_decoration_v1_listener decorationListener = {
            .configure = Method( DecorationConfigure )
        };

        m_xdgToplevelDecoration = zxdg_decoration_manager_v1_get_toplevel_decoration( p.decorationManager, m_xdgToplevel );
        zxdg_toplevel_decoration_v1_add_listener( m_xdgToplevelDecoration, &decorationListener, this );
        zxdg_toplevel_decoration_v1_set_mode( m_xdgToplevelDecoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE );
    }
}

WaylandWindow::~WaylandWindow()
{
    if( m_viewport ) wp_viewport_destroy( m_viewport );
    if( m_fractionalScale ) wp_fractional_scale_v1_destroy( m_fractionalScale );
    if( m_xdgToplevelDecoration ) zxdg_toplevel_decoration_v1_destroy( m_xdgToplevelDecoration );
    xdg_toplevel_destroy( m_xdgToplevel );
    xdg_surface_destroy( m_xdgSurface );
    wl_surface_destroy( m_surface );
}

void WaylandWindow::XdgSurfaceConfigure( struct xdg_surface *xdg_surface, uint32_t serial )
{
    xdg_surface_ack_configure( xdg_surface, serial );
}

void WaylandWindow::XdgToplevelConfigure( struct xdg_toplevel* toplevel, int32_t width, int32_t height, struct wl_array* states )
{
}

void WaylandWindow::XdgToplevelClose( struct xdg_toplevel* toplevel )
{
    //m_onClose();
}

void WaylandWindow::DecorationConfigure( zxdg_toplevel_decoration_v1* tldec, uint32_t mode )
{
}

void WaylandWindow::FractionalScalePreferredScale( wp_fractional_scale_v1* scale, uint32_t scaleValue )
{
    //mclog( LogLevel::Info, "Window %i scale: %g", m_id, scaleValue / 120.f );
    wp_viewport_set_source( m_viewport, 0, 0, wl_fixed_from_int( 1650 ), wl_fixed_from_int( 1050 ) );
    wp_viewport_set_destination( m_viewport, 1650 * 120 / scaleValue, 1050 * 120 / scaleValue );
}
