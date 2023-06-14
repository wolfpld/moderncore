#include <assert.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_wayland.h>

#include "WaylandMethod.hpp"
#include "WaylandWindow.hpp"
#include "../../render/SoftwareCursor.hpp"
#include "../../util/Panic.hpp"
#include "../../vulkan/VlkError.hpp"

WaylandWindow::WaylandWindow( wl_compositor* compositor, xdg_wm_base* xdgWmBase, zxdg_decoration_manager_v1* decorationManager, wl_display* dpy, VkInstance vkInstance, std::function<void()> onClose )
    : m_surface( wl_compositor_create_surface( compositor ) )
    , m_onClose( std::move( onClose ) )
    , m_vkInstance( vkInstance )
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

    wl_surface_commit( m_surface );
    wl_display_roundtrip( dpy );

    static constexpr wl_callback_listener frameListener = {
        .done = Method( WaylandWindow, FrameDone )
    };

    auto cb = wl_surface_frame( m_surface );
    wl_callback_add_listener( cb, &frameListener, this );

    if( decorationManager ) zxdg_decoration_manager_v1_get_toplevel_decoration( decorationManager, m_xdgToplevel );

    VkWaylandSurfaceCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR };
    createInfo.display = dpy;
    createInfo.surface = m_surface;

    VkVerify( vkCreateWaylandSurfaceKHR( vkInstance, &createInfo, nullptr, &m_vkSurface ) );
}

WaylandWindow::~WaylandWindow()
{
    vkDestroySurfaceKHR( m_vkInstance, m_vkSurface, nullptr );
    xdg_toplevel_destroy( m_xdgToplevel );
    xdg_surface_destroy( m_xdgSurface );
    wl_surface_destroy( m_surface );
}

void WaylandWindow::VulkanInit( const VlkDevice& device, VkRenderPass renderPass, uint32_t width, uint32_t height )
{
    m_cursor = std::make_unique<SoftwareCursor>( device, renderPass, width, height );
}

void WaylandWindow::Show( const std::function<void()>& render )
{
    assert( !m_onRender );
    m_onRender = render;
    render();
    wl_surface_commit( m_surface );
}

void WaylandWindow::RenderCursor( VkCommandBuffer cmdBuf )
{
    m_cursor->Render( cmdBuf );
}

void WaylandWindow::PointerMotion( double x, double y )
{
    m_cursor->SetPosition( x, y );
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

void WaylandWindow::FrameDone( struct wl_callback* cb, uint32_t time )
{
    wl_callback_destroy( cb );

    static constexpr wl_callback_listener frameListener = {
        .done = Method( WaylandWindow, FrameDone )
    };

    cb = wl_surface_frame( m_surface );
    wl_callback_add_listener( cb, &frameListener, this );

    m_onRender();
}
