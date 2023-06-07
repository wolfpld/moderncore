#include "WaylandMethod.hpp"
#include "WaylandWindow.hpp"
#include "../../util/Panic.hpp"

WaylandWindow::WaylandWindow( wl_compositor* compositor, xdg_wm_base* xdgWmBase, zxdg_decoration_manager_v1* decorationManager, std::function<void()> onClose )
    : m_surface( wl_compositor_create_surface( compositor ) )
    , m_onClose( std::move( onClose ) )
{
    CheckPanic( m_surface, "Failed to create Wayland surface" );

    static constexpr xdg_surface_listener xdgSurfaceListener = {
        .configure = Method( WaylandWindow, XdgSurfaceConfigure )
    };

    m_xdgSurface = xdg_wm_base_get_xdg_surface( xdgWmBase, m_surface );
    CheckPanic( m_xdgSurface, "Failed to create Wayland xdg_surface" );
    xdg_surface_add_listener( m_xdgSurface, &xdgSurfaceListener, this );

    static constexpr xdg_toplevel_listener toplevelListener = {
        .configure = Method( WaylandWindow, XdgToplevelConfigure ),
        .close = Method( WaylandWindow, XdgToplevelClose )
    };

    m_xdgToplevel = xdg_surface_get_toplevel( m_xdgSurface );
    CheckPanic( m_xdgToplevel, "Failed to create Wayland xdg_toplevel" );
    xdg_toplevel_add_listener( m_xdgToplevel, &toplevelListener, this );
    xdg_toplevel_set_title( m_xdgToplevel, "ModernCore" );
    xdg_toplevel_set_app_id( m_xdgToplevel, "moderncore" );

    if( decorationManager ) zxdg_decoration_manager_v1_get_toplevel_decoration( decorationManager, m_xdgToplevel );
}

WaylandWindow::~WaylandWindow()
{
    xdg_toplevel_destroy( m_xdgToplevel );
    xdg_surface_destroy( m_xdgSurface );
    wl_surface_destroy( m_surface );
}

void WaylandWindow::Show()
{
    wl_surface_commit( m_surface );
}

void WaylandWindow::XdgSurfaceConfigure( struct xdg_surface *xdg_surface, uint32_t serial )
{
    xdg_surface_ack_configure( xdg_surface, serial );
}

void WaylandWindow::XdgToplevelConfigure( struct xdg_toplevel* toplevel, int32_t width, int32_t height, struct wl_array* states )
{
    if( width == 0 || height == 0 ) return;

    wl_surface_commit( m_surface );
}

void WaylandWindow::XdgToplevelClose( struct xdg_toplevel* toplevel )
{
    m_onClose();
}
