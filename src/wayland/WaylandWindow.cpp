#include <tracy/Tracy.hpp>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_wayland.h>

#include "WaylandDisplay.hpp"
#include "WaylandMethod.hpp"
#include "WaylandWindow.hpp"
#include "util/Panic.hpp"
#include "vulkan/VlkError.hpp"
#include "vulkan/VlkInstance.hpp"

WaylandWindow::WaylandWindow( WaylandDisplay& display, VlkInstance& vkInstance )
    : m_vkInstance( vkInstance )
{
    ZoneScoped;

    m_surface = wl_compositor_create_surface( display.Compositor() );
    CheckPanic( m_surface, "Failed to create Wayland surface" );

    if( display.FractionalScaleManager() && display.Viewporter() )
    {
        static constexpr wp_fractional_scale_v1_listener listener = {
            .preferred_scale = Method( FractionalScalePreferredScale )
        };

        m_fractionalScale = wp_fractional_scale_manager_v1_get_fractional_scale( display.FractionalScaleManager(), m_surface );
        CheckPanic( m_fractionalScale, "Failed to create Wayland fractional scale" );
        wp_fractional_scale_v1_add_listener( m_fractionalScale, &listener, this );

        m_viewport = wp_viewporter_get_viewport( display.Viewporter(), m_surface );
        CheckPanic( m_viewport, "Failed to create Wayland viewport" );
    }

    static constexpr xdg_surface_listener xdgSurfaceListener = {
        .configure = Method( XdgSurfaceConfigure )
    };

    m_xdgSurface = xdg_wm_base_get_xdg_surface( display.XdgWmBase(), m_surface );
    CheckPanic( m_xdgSurface, "Failed to create Wayland xdg_surface" );
    xdg_surface_add_listener( m_xdgSurface, &xdgSurfaceListener, this );

    static constexpr xdg_toplevel_listener toplevelListener = {
        .configure = Method( XdgToplevelConfigure ),
        .close = Method( XdgToplevelClose )
    };

    m_xdgToplevel = xdg_surface_get_toplevel( m_xdgSurface );
    CheckPanic( m_xdgToplevel, "Failed to create Wayland xdg_toplevel" );
    xdg_toplevel_add_listener( m_xdgToplevel, &toplevelListener, this );

    if( display.DecorationManager() )
    {
        static constexpr zxdg_toplevel_decoration_v1_listener decorationListener = {
            .configure = Method( DecorationConfigure )
        };

        m_xdgToplevelDecoration = zxdg_decoration_manager_v1_get_toplevel_decoration( display.DecorationManager(), m_xdgToplevel );
        zxdg_toplevel_decoration_v1_add_listener( m_xdgToplevelDecoration, &decorationListener, this );
        zxdg_toplevel_decoration_v1_set_mode( m_xdgToplevelDecoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE );
    }

    VkWaylandSurfaceCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR };
    createInfo.display = display.Display();
    createInfo.surface = Surface();

    VkVerify( vkCreateWaylandSurfaceKHR( vkInstance, &createInfo, nullptr, &m_vkSurface ) );
}

WaylandWindow::~WaylandWindow()
{
    vkDestroySurfaceKHR( m_vkInstance, m_vkSurface, nullptr );
    if( m_viewport ) wp_viewport_destroy( m_viewport );
    if( m_fractionalScale ) wp_fractional_scale_v1_destroy( m_fractionalScale );
    if( m_xdgToplevelDecoration ) zxdg_toplevel_decoration_v1_destroy( m_xdgToplevelDecoration );
    xdg_toplevel_destroy( m_xdgToplevel );
    xdg_surface_destroy( m_xdgSurface );
    wl_surface_destroy( m_surface );
}

void WaylandWindow::SetListener( const Listener* listener, void* listenerPtr )
{
    m_listener = listener;
    m_listenerPtr = listenerPtr;
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
    if( m_listener->OnClose ) m_listener->OnClose( m_listenerPtr, this );
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
