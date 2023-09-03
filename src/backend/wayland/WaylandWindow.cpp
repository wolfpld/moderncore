#include <assert.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_wayland.h>

#include "WaylandConnector.hpp"
#include "WaylandMethod.hpp"
#include "WaylandWindow.hpp"
#include "../../render/SoftwareCursor.hpp"
#include "../../server/GpuState.hpp"
#include "../../util/Panic.hpp"
#include "../../vulkan/PhysDevSel.hpp"
#include "../../vulkan/VlkDevice.hpp"
#include "../../vulkan/VlkError.hpp"
#include "../../vulkan/VlkInstance.hpp"

WaylandWindow::WaylandWindow( Params&& p )
    : m_surface( wl_compositor_create_surface( p.compositor ) )
    , m_onClose( std::move( p.onClose ) )
    , m_vkInstance( p.vkInstance )
    , m_gpuState( p.gpuState )
{
    CheckPanic( m_surface, "Failed to create Wayland surface" );

    static constexpr xdg_surface_listener xdgSurfaceListener = {
        .configure = Method( WaylandWindow, XdgSurfaceConfigure )
    };

    m_xdgSurface = xdg_wm_base_get_xdg_surface( p.xdgWmBase, m_surface );
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

    if( p.decorationManager ) zxdg_decoration_manager_v1_get_toplevel_decoration( p.decorationManager, m_xdgToplevel );

    wl_surface_commit( m_surface );
    wl_display_roundtrip( p.dpy );

    static constexpr wl_callback_listener frameListener = {
        .done = Method( WaylandWindow, FrameDone )
    };

    auto cb = wl_surface_frame( m_surface );
    wl_callback_add_listener( cb, &frameListener, this );

    VkWaylandSurfaceCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR };
    createInfo.display = p.dpy;
    createInfo.surface = m_surface;

    VkVerify( vkCreateWaylandSurfaceKHR( m_vkInstance, &createInfo, nullptr, &m_vkSurface ) );

    auto device = PhysDevSel::PickBest( m_vkInstance.QueryPhysicalDevices(), m_vkSurface );
    CheckPanic( device != VK_NULL_HANDLE, "Failed to find suitable physical device" );

    m_vkDevice = p.gpuState.Devices().Get( device );
    if( !m_vkDevice ) m_vkDevice = p.gpuState.Devices().Add( device, m_vkSurface );
    assert( m_vkDevice );

    m_connectorId = p.gpuState.Connectors().Add( std::make_shared<WaylandConnector>( *m_vkDevice, m_vkSurface ) );
}

WaylandWindow::~WaylandWindow()
{
    m_gpuState.Connectors().Remove( m_connectorId );
    m_vkDevice.reset();
    vkDestroySurfaceKHR( m_vkInstance, m_vkSurface, nullptr );
    xdg_toplevel_destroy( m_xdgToplevel );
    xdg_surface_destroy( m_xdgSurface );
    wl_surface_destroy( m_surface );
}

/*
void WaylandWindow::VulkanInit( VlkDevice& device, VkRenderPass renderPass, uint32_t width, uint32_t height )
{
    m_cursor = std::make_unique<SoftwareCursor>( device, renderPass, width, height );
}
*/

void WaylandWindow::Show( const std::function<void()>& render )
{
    assert( !m_onRender );
    m_onRender = render;
    render();
    wl_surface_commit( m_surface );
}

void WaylandWindow::SetScale( int32_t scale )
{
    wl_surface_set_buffer_scale( m_surface, scale );
}

void WaylandWindow::RenderCursor( VkCommandBuffer cmdBuf, CursorLogic& cursorLogic )
{
    m_cursor->Render( cmdBuf, cursorLogic );
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
