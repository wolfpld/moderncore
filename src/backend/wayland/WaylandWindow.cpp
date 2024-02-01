#include <assert.h>
#include <format>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_wayland.h>

#include "BackendWayland.hpp"
#include "WaylandConnector.hpp"
#include "WaylandMethod.hpp"
#include "WaylandOutput.hpp"
#include "WaylandWindow.hpp"
#include "render/SoftwareCursor.hpp"
#include "util/Config.hpp"
#include "util/Panic.hpp"
#include "vulkan/PhysDevSel.hpp"
#include "vulkan/VlkError.hpp"
#include "vulkan/VlkInstance.hpp"

WaylandWindow::WaylandWindow( Params&& p )
    : m_surface( wl_compositor_create_surface( p.compositor ) )
    , m_onClose( std::move( p.onClose ) )
    , m_backend( p.backend )
    , m_vkInstance( p.vkInstance )
{
    CheckPanic( m_surface, "Failed to create Wayland surface" );

    static constexpr wl_surface_listener surfaceListener = {
        .enter = Method( Enter ),
        .leave = Method( Leave )
    };

    wl_surface_add_listener( m_surface, &surfaceListener, this );

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

    static int windowCount = 0;

    m_xdgToplevel = xdg_surface_get_toplevel( m_xdgSurface );
    CheckPanic( m_xdgToplevel, "Failed to create Wayland xdg_toplevel" );
    xdg_toplevel_add_listener( m_xdgToplevel, &toplevelListener, this );
    xdg_toplevel_set_title( m_xdgToplevel, std::format( "ModernCore #{}", ++windowCount ).c_str() );
    xdg_toplevel_set_app_id( m_xdgToplevel, "moderncore" );

    if( p.decorationManager )
    {
        static constexpr zxdg_toplevel_decoration_v1_listener decorationListener = {
            .configure = Method( DecorationConfigure )
        };

        m_xdgToplevelDecoration = zxdg_decoration_manager_v1_get_toplevel_decoration( p.decorationManager, m_xdgToplevel );
        zxdg_toplevel_decoration_v1_add_listener( m_xdgToplevelDecoration, &decorationListener, this );
        zxdg_toplevel_decoration_v1_set_mode( m_xdgToplevelDecoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE );
    }

    wl_surface_commit( m_surface );
    wl_display_roundtrip( p.dpy );

    static constexpr wl_callback_listener frameListener = {
        .done = Method( FrameDone )
    };

    auto cb = wl_surface_frame( m_surface );
    wl_callback_add_listener( cb, &frameListener, this );

    VkWaylandSurfaceCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR };
    createInfo.display = p.dpy;
    createInfo.surface = m_surface;

    VkVerify( vkCreateWaylandSurfaceKHR( m_vkInstance, &createInfo, nullptr, &m_vkSurface ) );

    Config config( "backend-wayland.ini" );
    const auto configPhysDev = config.Get( "Vulkan", "PhysicalDevice", -1 );

    VkPhysicalDevice device;
    const auto physicalDevices = m_vkInstance.QueryPhysicalDevices();
    if( configPhysDev < 0 )
    {
        device = PhysDevSel::PickBest( physicalDevices, m_vkSurface );
        CheckPanic( device != VK_NULL_HANDLE, "Failed to find suitable physical device" );
    }
    else
    {
        CheckPanic( configPhysDev < physicalDevices.size(), "Invalid physical device index set in backend-wayland.ini. Value: %i, max: %zu.", configPhysDev, physicalDevices.size() - 1 );
        device = physicalDevices[configPhysDev];
    }

/*
    m_vkDevice = p.gpuState.Devices().Get( device );
    if( !m_vkDevice ) m_vkDevice = p.gpuState.Devices().Add( device, m_vkSurface );
    assert( m_vkDevice );

    m_connectorId = p.gpuState.Connectors().Add( std::make_shared<WaylandConnector>( *m_vkDevice, m_vkSurface ) );
*/
}

WaylandWindow::~WaylandWindow()
{
    //m_gpuState.Connectors().Remove( m_connectorId );
    m_vkDevice.reset();
    vkDestroySurfaceKHR( m_vkInstance, m_vkSurface, nullptr );
    if( m_xdgToplevelDecoration ) zxdg_toplevel_decoration_v1_destroy( m_xdgToplevelDecoration );
    xdg_toplevel_destroy( m_xdgToplevel );
    xdg_surface_destroy( m_xdgSurface );
    wl_surface_destroy( m_surface );
}

void WaylandWindow::Show( const std::function<void()>& render )
{
    assert( !m_onRender );
    m_onRender = render;
    render();
}

void WaylandWindow::RenderCursor( VkCommandBuffer cmdBuf, CursorLogic& cursorLogic )
{
    m_cursor->Render( cmdBuf, cursorLogic );
}

void WaylandWindow::PointerMotion( double x, double y )
{
    m_cursor->SetPosition( x, y );
}

void WaylandWindow::Enter( struct wl_surface* surface, struct wl_output* output )
{
    auto& outputMap = m_backend.OutputMap();
    auto it = std::find_if( outputMap.begin(), outputMap.end(), [output]( const auto& pair ) { return pair.second->GetOutput() == output; } );
    assert( it != outputMap.end() );
    m_outputs.insert( it->first );

    const auto outputScale = it->second->GetScale();
    if( outputScale > m_scale )
    {
        mclog( LogLevel::Info, "Window %i: Enter output %i, setting scale to %i", m_connectorId, it->first, outputScale );
        m_scale = outputScale;
        wl_surface_set_buffer_scale( m_surface, m_scale );
        wl_surface_commit( m_surface );
    }
}

void WaylandWindow::Leave( struct wl_surface* surface, struct wl_output* output )
{
    auto& outputMap = m_backend.OutputMap();
    auto it = std::find_if( outputMap.begin(), outputMap.end(), [output]( const auto& pair ) { return pair.second->GetOutput() == output; } );
    assert( it != outputMap.end() );
    m_outputs.erase( it->first );

    if( it->second->GetScale() == m_scale )
    {
        int scale = 1;
        for( const auto& outputId : m_outputs )
        {
            const auto oit = outputMap.find( outputId );
            assert( oit != outputMap.end() );
            const auto outputScale = oit->second->GetScale();
            if( outputScale > scale ) scale = outputScale;
        }
        if( scale != m_scale )
        {
            mclog( LogLevel::Info, "Window %i: Leave output %i, setting scale to %i", m_connectorId, it->first, scale );
            m_scale = scale;
            wl_surface_set_buffer_scale( m_surface, m_scale );
            wl_surface_commit( m_surface );
        }
    }
}

void WaylandWindow::XdgSurfaceConfigure( struct xdg_surface *xdg_surface, uint32_t serial )
{
    xdg_surface_ack_configure( xdg_surface, serial );
}

void WaylandWindow::XdgToplevelConfigure( struct xdg_toplevel* toplevel, int32_t width, int32_t height, struct wl_array* states )
{
    if( width == 0 || height == 0 ) return;
}

void WaylandWindow::XdgToplevelClose( struct xdg_toplevel* toplevel )
{
    m_onClose();
}

void WaylandWindow::FrameDone( struct wl_callback* cb, uint32_t time )
{
    wl_callback_destroy( cb );

    static constexpr wl_callback_listener listener = {
        .done = Method( FrameDone )
    };

    cb = wl_surface_frame( m_surface );
    wl_callback_add_listener( cb, &listener, this );

    m_onRender();
}

void WaylandWindow::DecorationConfigure( zxdg_toplevel_decoration_v1* tldec, uint32_t mode )
{
}
