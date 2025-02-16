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
#include "vulkan/VlkGarbage.hpp"
#include "vulkan/VlkInstance.hpp"
#include "vulkan/VlkSemaphore.hpp"
#include "vulkan/VlkSwapchain.hpp"

WaylandWindow::WaylandWindow( WaylandDisplay& display, VlkInstance& vkInstance )
    : m_vkInstance( vkInstance )
    , m_garbage( std::make_shared<VlkGarbage>() )
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

    m_vkSurface = std::make_shared<VlkSurface>( m_vkInstance, display.Display(), m_surface );
}

WaylandWindow::~WaylandWindow()
{
    CleanupSwapchain( true );

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
    m_extent = {
        .width = width,
        .height = height
    };

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
    const auto& extent = m_swapchain->GetExtent();
    if( extent.width != m_extent.width || extent.height != m_extent.height )
    {
        CreateSwapchain( m_extent );
    }

    if( m_scale != m_prevScale )
    {
        m_prevScale = m_scale;
        wp_viewport_set_source( m_viewport, 0, 0, wl_fixed_from_int( m_extent.width ), wl_fixed_from_int( m_extent.height ) );
        wp_viewport_set_destination( m_viewport, m_extent.width * 120 / m_scale, m_extent.height * 120 / m_scale );
    }

    m_frameIdx = ( m_frameIdx + 1 ) % m_frameData.size();
    auto& frame = m_frameData[m_frameIdx];

    frame.renderFence->Wait();
    for(;;)
    {
        auto res = vkAcquireNextImageKHR( *m_vkDevice, *m_swapchain, UINT64_MAX, *frame.imageAvailable, VK_NULL_HANDLE, &m_imageIdx );
        if( res == VK_SUCCESS ) break;
        if( res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR )
        {
            mclog( LogLevel::Warning, "Swapchain out of date or suboptimal, recreating (%s)", string_VkResult( res ) );
            CreateSwapchain( m_extent );
        }
        else
        {
            Panic( "Failed to acquire swapchain image (%s)", string_VkResult( res ) );
        }
    }

    frame.renderFence->Reset();
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

    VkVerify( vkQueueSubmit( m_vkDevice->GetQueue( QueueType::Graphic ), 1, &submitInfo, *frame.renderFence ) );

    const std::array<VkSwapchainKHR, 1> swapchains = { *m_swapchain };
    const std::array<VkFence, 1> fences = { *frame.presentFence };
    static_assert( swapchains.size() == fences.size() );

    frame.presentFence->Wait();
    frame.presentFence->Reset();

    VkSwapchainPresentFenceInfoEXT presentFenceInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT };
    presentFenceInfo.swapchainCount = fences.size();
    presentFenceInfo.pFences = fences.data();

    VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    presentInfo.pNext = &presentFenceInfo;
    presentInfo.waitSemaphoreCount = signalSemaphores.size();
    presentInfo.pWaitSemaphores = signalSemaphores.data();
    presentInfo.swapchainCount = swapchains.size();
    presentInfo.pSwapchains = swapchains.data();
    presentInfo.pImageIndices = &m_imageIdx;

    const auto res = vkQueuePresentKHR( m_vkDevice->GetQueue( QueueType::Present ), &presentInfo );
    CheckPanic( res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR, "Failed to present swapchain image (%s)", string_VkResult( res ) );
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
    static constexpr wl_callback_listener listener = {
        .done = Method( FrameDone )
    };

    auto cb = wl_surface_frame( m_surface );
    wl_callback_add_listener( cb, &listener, this );

    if( !InvokeRet( OnRender, false, this ) ) wl_surface_commit( m_surface );

    m_garbage->Collect();
}

void WaylandWindow::Recycle( std::shared_ptr<VlkBase>&& garbage )
{
    auto& current = m_frameData[m_frameIdx];
    m_garbage->Recycle( current.renderFence, std::move( garbage ) );
}

void WaylandWindow::Recycle( std::vector<std::shared_ptr<VlkBase>>&& garbage )
{
    auto& current = m_frameData[m_frameIdx];
    m_garbage->Recycle( current.renderFence, std::move( garbage ) );
}

void WaylandWindow::CreateSwapchain( const VkExtent2D& extent )
{
    auto oldSwapchain = m_swapchain;
    if( m_swapchain ) CleanupSwapchain();
    m_swapchain = std::make_shared<VlkSwapchain>( *m_vkDevice, *m_vkSurface, extent, oldSwapchain ? *oldSwapchain : VkSwapchainKHR { VK_NULL_HANDLE } );
    m_extent = m_swapchain->GetExtent();
    oldSwapchain.reset();

    const auto imageViews = m_swapchain->GetImageViews();
    const auto numImages = imageViews.size();

    CheckPanic( m_frameData.empty(), "Frame data is not empty!" );
    m_frameData.reserve( numImages );
    for( size_t i=0; i<numImages; i++ )
    {
        m_frameData.emplace_back( FrameData {
            .commandBuffer = std::make_shared<VlkCommandBuffer>( *m_vkDevice->GetCommandPool( QueueType::Graphic ), true ),
            .imageAvailable = std::make_shared<VlkSemaphore>( *m_vkDevice ),
            .renderFinished = std::make_shared<VlkSemaphore>( *m_vkDevice ),
            .renderFence = std::make_shared<VlkFence>( *m_vkDevice, VK_FENCE_CREATE_SIGNALED_BIT ),
            .presentFence = std::make_shared<VlkFence>( *m_vkDevice, VK_FENCE_CREATE_SIGNALED_BIT )
        } );
    }

    m_garbage->Collect();
}

void WaylandWindow::CleanupSwapchain( bool withSurface )
{
    auto& current = m_frameData[m_frameIdx];
    m_garbage->Recycle( std::move( current.renderFence ), current.commandBuffer );
    std::vector<std::shared_ptr<VlkBase>> objects = {
        current.imageAvailable,
        current.renderFinished,
        std::move( m_swapchain ),
    };
    if( withSurface ) objects.emplace_back( std::move( m_vkSurface ) );
    m_garbage->Recycle( std::move( current.presentFence ), std::move( objects ) );

    uint32_t idx = (m_frameIdx + 1) % m_frameData.size();
    while( idx != m_frameIdx )
    {
        auto& frame = m_frameData[idx];
        m_garbage->Recycle( std::move( frame.renderFence ), frame.commandBuffer );
        m_garbage->Recycle( std::move( frame.presentFence ), {
            frame.imageAvailable,
            frame.renderFinished
        } );
        idx = (idx + 1) % m_frameData.size();
    }

    m_frameData.clear();
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
    InvokeRender();
}
