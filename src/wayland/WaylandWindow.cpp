#include <math.h>
#include <tracy/Tracy.hpp>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_wayland.h>

#include "WaylandDisplay.hpp"
#include "WaylandWindow.hpp"
#include "util/Invoke.hpp"
#include "util/Panic.hpp"
#include "vulkan/VlkCommandBuffer.hpp"
#include "vulkan/VlkDevice.hpp"
#include "vulkan/VlkError.hpp"
#include "vulkan/VlkFence.hpp"
#include "vulkan/VlkInstance.hpp"
#include "vulkan/VlkSemaphore.hpp"
#include "vulkan/VlkSwapchain.hpp"

WaylandWindow::WaylandWindow( WaylandDisplay& display, VlkInstance& vkInstance )
    : m_vkInstance( vkInstance )
{
    ZoneScoped;

    m_surface = wl_compositor_create_surface( display.Compositor() );
    CheckPanic( m_surface, "Failed to create Wayland surface" );

    static constexpr wp_fractional_scale_v1_listener listener = {
        .preferred_scale = Method( FractionalScalePreferredScale )
    };

    m_fractionalScale = wp_fractional_scale_manager_v1_get_fractional_scale( display.FractionalScaleManager(), m_surface );
    CheckPanic( m_fractionalScale, "Failed to create Wayland fractional scale" );
    wp_fractional_scale_v1_add_listener( m_fractionalScale, &listener, this );

    m_viewport = wp_viewporter_get_viewport( display.Viewporter(), m_surface );
    CheckPanic( m_viewport, "Failed to create Wayland viewport" );

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

    static constexpr wl_callback_listener frameListener = {
        .done = Method( FrameDone )
    };

    auto cb = wl_surface_frame( m_surface );
    wl_callback_add_listener( cb, &frameListener, this );
}

WaylandWindow::~WaylandWindow()
{
    vkQueueWaitIdle( m_vkDevice->GetQueue( QueueType::Graphic ) );
    for( auto& frame : m_frameData ) frame.fence->Wait();
    if( m_swapchain ) m_swapchain.reset();
    vkDestroySurfaceKHR( m_vkInstance, m_vkSurface, nullptr );
    wp_viewport_destroy( m_viewport );
    wp_fractional_scale_v1_destroy( m_fractionalScale );
    if( m_xdgToplevelDecoration ) zxdg_toplevel_decoration_v1_destroy( m_xdgToplevelDecoration );
    xdg_toplevel_destroy( m_xdgToplevel );
    xdg_surface_destroy( m_xdgSurface );
    wl_surface_destroy( m_surface );
}

void WaylandWindow::SetAppId( const char* appId )
{
    xdg_toplevel_set_app_id( m_xdgToplevel, appId );
}

void WaylandWindow::SetTitle( const char* title )
{
    m_title = title;
    xdg_toplevel_set_title( m_xdgToplevel, title );
}

void WaylandWindow::Resize( uint32_t width, uint32_t height )
{
    CreateSwapchain( VkExtent2D( width, height ) );

    // Ensure viewport is updated
    m_prevScale = 0;
}

void WaylandWindow::LockSize()
{
    CheckPanic( m_swapchain, "Swapchain not created" );

    const auto& extent = m_swapchain->GetExtent();
    xdg_toplevel_set_min_size( m_xdgToplevel, extent.width, extent.height );
    xdg_toplevel_set_max_size( m_xdgToplevel, extent.width, extent.height );
}

void WaylandWindow::Commit()
{
    wl_surface_commit( m_surface );
}

VlkCommandBuffer& WaylandWindow::BeginFrame( bool imageTransfer )
{
    if( m_scale != m_prevScale )
    {
        m_prevScale = m_scale;
        const auto& extent = m_swapchain->GetExtent();
        wp_viewport_set_source( m_viewport, 0, 0, wl_fixed_from_int( extent.width ), wl_fixed_from_int( extent.height ) );
        wp_viewport_set_destination( m_viewport, extent.width * 120 / m_scale, extent.height * 120 / m_scale );
    }

    auto& frame = m_frameData[m_frameIdx];

    frame.fence->Wait();
    frame.fence->Reset();

    auto res = vkAcquireNextImageKHR( *m_vkDevice, *m_swapchain, UINT64_MAX, *frame.imageAvailable, VK_NULL_HANDLE, &m_imageIdx );

    frame.commandBuffer->Reset();
    frame.commandBuffer->Begin( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );

    if( imageTransfer )
    {
        m_swapchain->TransferBarrier( *frame.commandBuffer, m_imageIdx );
    }
    else
    {
        m_swapchain->RenderBarrier( *frame.commandBuffer, m_imageIdx );
    }

    return *frame.commandBuffer;
}

void WaylandWindow::EndFrame()
{
    auto& frame = m_frameData[m_frameIdx];

    m_swapchain->PresentBarrier( *frame.commandBuffer, m_imageIdx );

#ifdef TRACY_ENABLE
    { auto tracyCtx = m_vkDevice->GetTracyContext(); if( tracyCtx ) TracyVkCollect( tracyCtx, *frame.commandBuffer ); }
#endif

    frame.commandBuffer->End();

    const std::array<VkSemaphore, 1> waitSemaphores = { *frame.imageAvailable };
    const std::array<VkSemaphore, 1> signalSemaphores = { *frame.renderFinished };
    constexpr std::array<VkPipelineStageFlags, 1> waitStages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    const std::array<VkCommandBuffer, 1> commandBuffers = { *frame.commandBuffer };

    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.waitSemaphoreCount = waitSemaphores.size();
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.pWaitDstStageMask = waitStages.data();
    submitInfo.commandBufferCount = commandBuffers.size();
    submitInfo.pCommandBuffers = commandBuffers.data();
    submitInfo.signalSemaphoreCount = signalSemaphores.size();
    submitInfo.pSignalSemaphores = signalSemaphores.data();

    VkVerify( vkQueueSubmit( m_vkDevice->GetQueue( QueueType::Graphic ), 1, &submitInfo, *frame.fence ) );

    const std::array<VkSwapchainKHR, 1> swapchains = { *m_swapchain };

    VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    presentInfo.waitSemaphoreCount = signalSemaphores.size();
    presentInfo.pWaitSemaphores = signalSemaphores.data();
    presentInfo.swapchainCount = swapchains.size();
    presentInfo.pSwapchains = swapchains.data();
    presentInfo.pImageIndices = &m_imageIdx;

    VkVerify( vkQueuePresentKHR( m_vkDevice->GetQueue( QueueType::Present ), &presentInfo ) );

    m_frameIdx = ( m_frameIdx + 1 ) % m_frameData.size();
}

VkImage WaylandWindow::GetImage()
{
    return m_swapchain->GetImages()[m_imageIdx];
}

VkImageView WaylandWindow::GetImageView()
{
    return m_swapchain->GetImageViews()[m_imageIdx];
}

void WaylandWindow::SetListener( const Listener* listener, void* listenerPtr )
{
    m_listener = listener;
    m_listenerPtr = listenerPtr;
}

void WaylandWindow::SetDevice( std::shared_ptr<VlkDevice> device, const VkExtent2D& extent )
{
    CheckPanic( !m_vkDevice, "Vulkan device already set" );
    m_vkDevice = std::move( device );
    CreateSwapchain( extent );
}

void WaylandWindow::InvokeRender()
{
    Invoke( OnRender, this );
}

void WaylandWindow::CreateSwapchain( const VkExtent2D& extent )
{
    m_swapchain.reset();
    m_swapchain = std::make_unique<VlkSwapchain>( *m_vkDevice, m_vkSurface, extent );

    const auto imageViews = m_swapchain->GetImageViews();
    const auto numImages = imageViews.size();
    const auto oldSize = m_frameData.size();

    if( oldSize == numImages ) return;

    m_frameData.resize( numImages );
    if( oldSize > numImages ) return;

    for( size_t i=oldSize; i<numImages; i++ )
    {
        m_frameData[i].commandBuffer = std::make_unique<VlkCommandBuffer>( *m_vkDevice->GetCommandPool( QueueType::Graphic ), true );
        m_frameData[i].imageAvailable = std::make_unique<VlkSemaphore>( *m_vkDevice );
        m_frameData[i].renderFinished = std::make_unique<VlkSemaphore>( *m_vkDevice );
        m_frameData[i].fence = std::make_unique<VlkFence>( *m_vkDevice, VK_FENCE_CREATE_SIGNALED_BIT );
    }
}

void WaylandWindow::XdgSurfaceConfigure( struct xdg_surface *xdg_surface, uint32_t serial )
{
    xdg_surface_ack_configure( xdg_surface, serial );
    if( m_stageWidth == 0 || m_stageHeight == 0 ) return;
    auto& extent = m_swapchain->GetExtent();
    const auto dx = abs( int32_t( extent.width ) - int32_t( m_stageWidth ) );
    const auto dy = abs( int32_t( extent.height ) - int32_t( m_stageHeight ) );
    if( dx > 1 || dy > 1 )
    {
        Invoke( OnResize, this, m_stageWidth, m_stageHeight );
    }
    m_stageWidth = m_stageHeight = 0;
}

void WaylandWindow::XdgToplevelConfigure( struct xdg_toplevel* toplevel, int32_t width, int32_t height, struct wl_array* states )
{
    if( width == 0 || height == 0 ) return;
    m_stageWidth = width * m_scale / 120;
    m_stageHeight = height * m_scale / 120;
}

void WaylandWindow::XdgToplevelClose( struct xdg_toplevel* toplevel )
{
    Invoke( OnClose, this );
}

void WaylandWindow::DecorationConfigure( zxdg_toplevel_decoration_v1* tldec, uint32_t mode )
{
}

void WaylandWindow::FractionalScalePreferredScale( wp_fractional_scale_v1* scale, uint32_t scaleValue )
{
    if( m_scale == scaleValue ) return;
    m_scale = scaleValue;
    Invoke( OnScale, this, scaleValue );
}

void WaylandWindow::FrameDone( struct wl_callback* cb, uint32_t time )
{
    wl_callback_destroy( cb );

    static constexpr wl_callback_listener listener = {
        .done = Method( FrameDone )
    };

    cb = wl_surface_frame( m_surface );
    wl_callback_add_listener( cb, &listener, this );

    InvokeRender();
}
