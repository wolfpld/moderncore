#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/mman.h>
#include <tracy/Tracy.hpp>
#include <unistd.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_wayland.h>

#include "WaylandDisplay.hpp"
#include "WaylandPointer.hpp"
#include "WaylandWindow.hpp"
#include "image/vector/SvgImage.hpp"
#include "util/Bitmap.hpp"
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
#include "wayland/WaylandCursor.hpp"

WaylandWindow::WaylandWindow( WaylandDisplay& display, VlkInstance& vkInstance )
    : m_display( display )
    , m_vkInstance( vkInstance )
    , m_cursor( WaylandCursor::Default )
{
    ZoneScoped;

    m_surface = wl_compositor_create_surface( display.Compositor() );
    CheckPanic( m_surface, "Failed to create Wayland surface" );
    m_display.Pointer().AddWindow( m_surface );

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
    m_display.Pointer().RemoveWindow( m_surface );
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

void WaylandWindow::SetIcon( const SvgImage& icon )
{
    const auto mgr = m_display.IconManager();
    if( !mgr ) return;
    const auto path = getenv( "XDG_RUNTIME_DIR" );
    if( !path ) return;
    const auto sizes = m_display.IconSizes();
    if( sizes.empty() ) return;
    const auto shm = m_display.Shm();

    size_t total = 0;
    for( auto sz : sizes ) total += sz * sz;
    total *= 4;

    std::string shmPath = path;
    shmPath.append( "/mcore_icon-XXXXXX" );
    int fd = mkstemp( shmPath.data() );
    if( fd < 0 ) return;
    unlink( shmPath.data() );
    ftruncate( fd, total );
    auto membuf = (char*)mmap( nullptr, total, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
    if( membuf == MAP_FAILED )
    {
        close( fd );
        return;
    }

    auto pool = wl_shm_create_pool( shm, fd, total );
    close( fd );
    auto wlicon = xdg_toplevel_icon_manager_v1_create_icon( mgr );

    std::vector<wl_buffer*> bufs;
    int32_t offset = 0;
    for( auto sz : sizes )
    {
        auto buf = wl_shm_pool_create_buffer( pool, offset, sz, sz, sz * 4, WL_SHM_FORMAT_ARGB8888 );
        bufs.push_back( buf );

        auto bmp = icon.Rasterize( sz, sz );
        auto src = (uint32_t*)bmp->Data();
        auto dst = (uint32_t*)(membuf + offset);
        offset += sz * sz * 4;

        int left = sz * sz;
        while( left-- )
        {
            uint32_t px = *src++;
            *dst++ = ( px & 0xFF00FF00 ) | ( px & 0x00FF0000 ) >> 16 | ( px & 0x000000FF ) << 16;
        }

        xdg_toplevel_icon_v1_add_buffer( wlicon, buf, sz );
    }

    xdg_toplevel_icon_manager_v1_set_icon( mgr, m_xdgToplevel, wlicon );
    xdg_toplevel_icon_v1_destroy( wlicon );
    for( auto buf : bufs ) wl_buffer_destroy( buf );
    munmap( membuf, total );
    wl_shm_pool_destroy( pool );
}

void WaylandWindow::Resize( uint32_t width, uint32_t height )
{
    m_staged = {
        .width = width,
        .height = height
    };
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

void WaylandWindow::Close()
{
    wl_surface_attach( m_surface, nullptr, 0, 0 );
    wl_surface_commit( m_surface );
    wl_display_flush( m_display.Display() );
}

VlkCommandBuffer& WaylandWindow::BeginFrame()
{
    if( m_prevScale == 0 ) m_prevScale = m_scale;
    const bool resized = m_staged.width != m_extent.width || m_staged.height != m_extent.height;
    if( resized || m_scale != m_prevScale )
    {
        if( resized ) Invoke( OnResize, this, m_staged.width, m_staged.height );

        m_extent = m_staged;
        m_prevScale = m_scale;

        CreateSwapchain( m_extent );

        wp_viewport_set_source( m_viewport, 0, 0, wl_fixed_from_double( m_extent.width * m_scale / 120.f ), wl_fixed_from_double( m_extent.height * m_scale / 120.f ) );
        wp_viewport_set_destination( m_viewport, m_extent.width, m_extent.height );
    }

    {
        std::lock_guard lock( m_cursorLock );
        if( m_cursor != m_display.Pointer().GetCursor( m_surface ) ) m_display.Pointer().SetCursor( m_surface, m_cursor );
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
    frame.commandBuffer->lock();
    frame.commandBuffer->Reset();
    frame.commandBuffer->Begin( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, false );
    m_swapchain->RenderBarrier( *frame.commandBuffer, m_imageIdx );

    return *frame.commandBuffer;
}

void WaylandWindow::EndFrame()
{
    auto& frame = m_frameData[m_frameIdx];

    m_swapchain->PresentBarrier( *frame.commandBuffer, m_imageIdx );

    TracyVkCollect( m_vkDevice->GetTracyContext(), *frame.commandBuffer );
    frame.commandBuffer->End();

    VkCommandBufferSubmitInfo cmdbufInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = *frame.commandBuffer
    };
    VkSemaphoreSubmitInfo semaImageAvailable = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = *frame.imageAvailable,
        .value = 1,
        .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    VkSemaphoreSubmitInfo semaRenderFinished = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = *frame.renderFinished,
        .value = 1,
        .stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT
    };
    VkSubmitInfo2 submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos = &semaImageAvailable,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmdbufInfo,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &semaRenderFinished
    };

    m_vkDevice->lock( QueueType::Graphic );
    VkVerify( vkQueueSubmit2( m_vkDevice->GetQueue( QueueType::Graphic ), 1, &submitInfo, *frame.renderFence ) );
    m_vkDevice->unlock( QueueType::Graphic );

    frame.presentFence->Wait();
    frame.presentFence->Reset();

    VkFence presentFence = *frame.presentFence;
    VkSemaphore renderFinished = *frame.renderFinished;
    VkSwapchainKHR swapchain = *m_swapchain;

    VkSwapchainPresentFenceInfoEXT presentFenceInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT,
        .swapchainCount = 1,
        .pFences = &presentFence
    };
    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = &presentFenceInfo,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderFinished,
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &m_imageIdx
    };
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

VkFormat WaylandWindow::GetFormat()
{
    return m_swapchain->GetFormat();
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
    m_staged = m_extent = extent;
    CreateSwapchain( extent );

    wp_viewport_set_source( m_viewport, 0, 0, wl_fixed_from_double( m_extent.width * m_scale / 120.f ), wl_fixed_from_double( m_extent.height * m_scale / 120.f ) );
    wp_viewport_set_destination( m_viewport, m_extent.width, m_extent.height );
}

void WaylandWindow::InvokeRender()
{
    static constexpr wl_callback_listener listener = {
        .done = Method( FrameDone )
    };

    auto cb = wl_surface_frame( m_surface );
    wl_callback_add_listener( cb, &listener, this );

    if( !InvokeRet( OnRender, false, this ) ) wl_surface_commit( m_surface );
}

void WaylandWindow::SetCursor( WaylandCursor cursor )
{
    std::lock_guard lock( m_cursorLock );
    m_cursor = cursor;
}

void WaylandWindow::Recycle( std::shared_ptr<VlkBase>&& garbage )
{
    auto& current = m_frameData[m_frameIdx];
    m_vkDevice->GetGarbage()->Recycle( current.renderFence, std::move( garbage ) );
}

void WaylandWindow::Recycle( std::vector<std::shared_ptr<VlkBase>>&& garbage )
{
    auto& current = m_frameData[m_frameIdx];
    m_vkDevice->GetGarbage()->Recycle( current.renderFence, std::move( garbage ) );
}

void WaylandWindow::CreateSwapchain( const VkExtent2D& extent )
{
    const auto scaled = VkExtent2D {
        .width = extent.width * m_scale / 120,
        .height = extent.height * m_scale / 120
    };

    auto oldSwapchain = m_swapchain;
    if( m_swapchain ) CleanupSwapchain();
    m_swapchain = std::make_shared<VlkSwapchain>( *m_vkDevice, *m_vkSurface, scaled, oldSwapchain ? *oldSwapchain : VkSwapchainKHR { VK_NULL_HANDLE } );
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
}

void WaylandWindow::CleanupSwapchain( bool withSurface )
{
    auto& garbage = *m_vkDevice->GetGarbage();
    auto& current = m_frameData[m_frameIdx];
    garbage.Recycle( std::move( current.renderFence ), current.commandBuffer );
    std::vector<std::shared_ptr<VlkBase>> objects = {
        current.imageAvailable,
        current.renderFinished,
        std::move( m_swapchain ),
    };
    if( withSurface ) objects.emplace_back( std::move( m_vkSurface ) );
    garbage.Recycle( std::move( current.presentFence ), std::move( objects ) );

    uint32_t idx = (m_frameIdx + 1) % m_frameData.size();
    while( idx != m_frameIdx )
    {
        auto& frame = m_frameData[idx];
        garbage.Recycle( std::move( frame.renderFence ), frame.commandBuffer );
        garbage.Recycle( std::move( frame.presentFence ), {
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
}

void WaylandWindow::XdgToplevelConfigure( struct xdg_toplevel* toplevel, int32_t width, int32_t height, struct wl_array* states )
{
    if( width == 0 || height == 0 ) return;

    m_staged = {
        .width = uint32_t( width ),
        .height = uint32_t( height )
    };
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
