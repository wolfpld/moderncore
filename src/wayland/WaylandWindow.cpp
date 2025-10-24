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
#include "WaylandSeat.hpp"
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
#include "vulkan/VlkPhysicalDevice.hpp"
#include "vulkan/VlkSemaphore.hpp"
#include "vulkan/VlkSwapchain.hpp"
#include "vulkan/VlkSwapchainFormats.hpp"
#include "wayland/WaylandCursor.hpp"
#include "wayland/WaylandOutput.hpp"

WaylandWindow::WaylandWindow( WaylandDisplay& display, VlkInstance& vkInstance )
    : m_display( display )
    , m_vkInstance( vkInstance )
    , m_hdrCapable( false )
    , m_bounds {}
    , m_idle( false )
    , m_cursor( WaylandCursor::Default )
{
    ZoneScoped;

    static constexpr wl_surface_listener surfaceListener = {
        .enter = Method( SurfaceEnter ),
        .leave = Method( SurfaceLeave ),
        .preferred_buffer_scale = Method( SurfacePreferredBufferScale ),
        .preferred_buffer_transform = Method( SurfacePreferredBufferTransform )
    };

    m_surface = wl_compositor_create_surface( display.Compositor() );
    CheckPanic( m_surface, "Failed to create Wayland surface" );
    m_display.Seat().AddWindow( this );
    wl_surface_add_listener( m_surface, &surfaceListener, this );

    static constexpr wp_fractional_scale_v1_listener fractionalListener = {
        .preferred_scale = Method( FractionalScalePreferredScale )
    };

    m_fractionalScale = wp_fractional_scale_manager_v1_get_fractional_scale( display.FractionalScaleManager(), m_surface );
    CheckPanic( m_fractionalScale, "Failed to create Wayland fractional scale" );
    wp_fractional_scale_v1_add_listener( m_fractionalScale, &fractionalListener, this );

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
        .close = Method( XdgToplevelClose ),
        .configure_bounds = Method( XdgToplevelConfigureBounds )
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
}

WaylandWindow::~WaylandWindow()
{
    CleanupSwapchain( true );
    if( m_surface ) Destroy();
}

void WaylandWindow::Destroy()
{
    CheckPanic( m_surface, "Window already destroyed" );

    wp_viewport_destroy( m_viewport );
    wp_fractional_scale_v1_destroy( m_fractionalScale );
    if( m_xdgToplevelDecoration ) zxdg_toplevel_decoration_v1_destroy( m_xdgToplevelDecoration );
    xdg_toplevel_destroy( m_xdgToplevel );
    xdg_surface_destroy( m_xdgSurface );
    m_display.Seat().RemoveWindow( this );
    wl_surface_destroy( m_surface );

    m_surface = nullptr;
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
    CheckPanic( total > 0, "Invalid icon sizes" );

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

        const auto bmp = icon.Rasterize( sz, sz );
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

static uint32_t ceil( uint32_t a, uint32_t b )
{
    return ( a + b - 1 ) / b;
}

void WaylandWindow::Resize( uint32_t width, uint32_t height, bool reposition )
{
    ResizeNoScale( ceil( width * 120, m_scale ), ceil( height * 120, m_scale ), reposition );
}

void WaylandWindow::ResizeNoScale( uint32_t width, uint32_t height, bool reposition )
{
    if( m_swapchain )
    {
        if( reposition && ( m_staged.width != width || m_staged.height != height ) )
        {
            // Does not work on KDE?
            const auto x = int32_t( m_staged.width - width ) / 2;
            const auto y = int32_t( m_staged.height - height ) / 2;
            wl_surface_offset( m_surface, x, y );
        }

        m_staged = {
            .width = width,
            .height = height
        };

        ResumeIfIdle();
    }
    else
    {
        m_floatingExtent = m_staged = m_extent = VkExtent2D( width, height );
        CreateSwapchain( m_extent );

        wp_viewport_set_destination( m_viewport, m_extent.width, m_extent.height );
    }
}

void WaylandWindow::LockSize()
{
    CheckPanic( m_swapchain, "Swapchain not created" );

    const auto& extent = m_swapchain->GetExtent();
    xdg_toplevel_set_min_size( m_xdgToplevel, extent.width, extent.height );
    xdg_toplevel_set_max_size( m_xdgToplevel, extent.width, extent.height );
}

void WaylandWindow::Maximize( bool enable )
{
    if( m_maximized == enable ) return;
    m_maximized = enable;

    if( enable )
    {
        xdg_toplevel_set_maximized( m_xdgToplevel );
    }
    else
    {
        xdg_toplevel_unset_maximized( m_xdgToplevel );
    }
}

void WaylandWindow::Fullscreen( bool enable )
{
    if( m_fullscreen == enable ) return;
    m_fullscreen = enable;

    if( enable )
    {
        xdg_toplevel_set_fullscreen( m_xdgToplevel, nullptr );
    }
    else
    {
        xdg_toplevel_unset_fullscreen( m_xdgToplevel );
    }
}

void WaylandWindow::Commit()
{
    wl_surface_commit( m_surface );
}

void WaylandWindow::Close()
{
    Destroy();
    wl_display_flush( m_display.Display() );
}

void WaylandWindow::Activate( const char* token )
{
    auto activation = m_display.Activation();
    if( !activation ) return;
    xdg_activation_v1_activate( activation, token, m_surface );
}

void WaylandWindow::EnableHdr( bool enable )
{
    if( !m_hdrCapable ) return;
    if( m_hdr != enable )
    {
        m_hdr = enable;
        ResumeIfIdle();
    }
}

void WaylandWindow::Update()
{
    m_stateLock.lock();
    if( m_prevScale == 0 ) m_prevScale = m_scale;
    const bool resized = m_staged.width != m_extent.width || m_staged.height != m_extent.height;
    const bool dpiChange = m_scale != m_prevScale;
    const bool hdrChange = m_hdr != m_prevHdr;
    if( resized || dpiChange || hdrChange )
    {
        m_extent = m_staged;
        m_prevScale = m_scale;
        m_prevHdr = m_hdr;
        if( !m_maximized && !m_fullscreen ) m_floatingExtent = m_extent;

        CreateSwapchain( m_extent );

        wp_viewport_set_destination( m_viewport, m_extent.width, m_extent.height );

        const auto extent = m_extent;
        const auto scale = m_scale;
        const auto format = m_swapchain->GetFormat();

        m_stateLock.unlock();

        if( dpiChange )
        {
            Invoke( OnScale, extent.width, extent.height, scale );
        }
        else if( resized )
        {
            Invoke( OnResize, extent.width, extent.height );
        }
        if( hdrChange ) Invoke( OnFormatChange, format );
    }
    else
    {
        m_stateLock.unlock();
    }

    if( m_prevMaxLuminance != m_maxLuminance )
    {
        Invoke( OnColor, m_maxLuminance );
        m_prevMaxLuminance = m_maxLuminance;
    }

    auto& seat = m_display.Seat();
    const auto cursor = m_cursor.load( std::memory_order_acquire );
    if( cursor != seat.GetCursor( m_surface ) ) seat.SetCursor( m_surface, cursor );
}

VlkCommandBuffer& WaylandWindow::BeginFrame()
{
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

    const VkCommandBufferSubmitInfo cmdbufInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = *frame.commandBuffer
    };
    const VkSemaphoreSubmitInfo semaImageAvailable = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = *frame.imageAvailable,
        .value = 1,
        .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    const VkSemaphoreSubmitInfo semaRenderFinished = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = *frame.renderFinished,
        .value = 1,
        .stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT
    };
    const VkSubmitInfo2 submitInfo = {
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

    m_currentRenderFence.store( frame.renderFence, std::memory_order_release );

    frame.presentFence->Wait();
    frame.presentFence->Reset();

    VkFence presentFence = *frame.presentFence;
    VkSemaphore renderFinished = *frame.renderFinished;
    VkSwapchainKHR swapchain = *m_swapchain;

    const VkSwapchainPresentFenceInfoEXT presentFenceInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT,
        .swapchainCount = 1,
        .pFences = &presentFence
    };
    const VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = &presentFenceInfo,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderFinished,
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &m_imageIdx
    };
    m_vkDevice->lock( QueueType::Present );
    const auto res = vkQueuePresentKHR( m_vkDevice->GetQueue( QueueType::Present ), &presentInfo );
    m_vkDevice->unlock( QueueType::Present );
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

void WaylandWindow::SetCursor( WaylandCursor cursor )
{
    m_cursor.store( cursor, std::memory_order_release );
}

void WaylandWindow::SetDevice( std::shared_ptr<VlkDevice> device )
{
    CheckPanic( !m_vkDevice, "Vulkan device already set" );
    m_vkDevice = std::move( device );

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR( *m_vkDevice->GetPhysicalDevice(), *m_vkSurface, &formatCount, nullptr );
    std::vector<VkSurfaceFormatKHR> m_formats( formatCount );
    vkGetPhysicalDeviceSurfaceFormatsKHR( *m_vkDevice->GetPhysicalDevice(), *m_vkSurface, &formatCount, m_formats.data() );

    const auto hdrFormat = FindSwapchainFormat( m_formats, HdrSwapchainFormats );
    m_hdrCapable = hdrFormat.format != VK_FORMAT_UNDEFINED;
}

void WaylandWindow::InvokeRender()
{
    static constexpr wl_callback_listener listener = {
        .done = Method( FrameDone )
    };

    auto cb = wl_surface_frame( m_surface );
    wl_callback_add_listener( cb, &listener, this );

    CheckPanic( !m_idle.load( std::memory_order_acquire ), "Window is rendering, but is idle?" );
    const auto idle = !InvokeRet( OnRender, false );
    if( idle ) m_idle.store( true, std::memory_order_release );
}

void WaylandWindow::InvokeClipboard( const unordered_flat_set<std::string>& mimeTypes )
{
    Invoke( OnClipboard, mimeTypes );
}

void WaylandWindow::InvokeDrag( const unordered_flat_set<std::string>& mimeTypes )
{
    Invoke( OnDrag, mimeTypes );
}

void WaylandWindow::InvokeDrop( int fd, const char* mime )
{
    Invoke( OnDrop, fd, mime );
}

void WaylandWindow::InvokeKeyEvent( uint32_t key, int mods, bool pressed )
{
    Invoke( OnKeyEvent, key, mods, pressed );
}

void WaylandWindow::InvokeCharacter( const char* character )
{
    Invoke( OnCharacter, character );
}

void WaylandWindow::InvokeMouseEnter( float x, float y )
{
    Invoke( OnMouseEnter, x * m_scale / 120, y * m_scale / 120 );
}

void WaylandWindow::InvokeMouseLeave()
{
    Invoke( OnMouseLeave );
}

void WaylandWindow::InvokeMouseMove( float x, float y )
{
    Invoke( OnMouseMove, x * m_scale / 120, y * m_scale / 120 );
}

void WaylandWindow::InvokeMouseButton( uint32_t button, bool pressed )
{
    Invoke( OnMouseButton, button, pressed );
}

void WaylandWindow::InvokeScroll( const WaylandScroll& scroll )
{
    Invoke( OnScroll, scroll );
}

void WaylandWindow::ResumeIfIdle()
{
    bool idle = m_idle.load( std::memory_order_relaxed );
    if( !idle ) return;
    while( !m_idle.compare_exchange_weak( idle, false, std::memory_order_release ) )
    {
        if( !idle ) return;
    }

    wl_surface_commit( m_surface );
}

int WaylandWindow::GetClipboard( const char* mime )
{
    return m_display.Seat().GetClipboard( mime );
}

int WaylandWindow::GetDnd( const char* mime )
{
    return m_display.Seat().GetDnd( mime );
}

void WaylandWindow::AcceptDndMime( const char* mime )
{
    m_display.Seat().AcceptDndMime( mime );
}

void WaylandWindow::FinishDnd( int fd )
{
    m_display.Seat().FinishDnd( fd );
}

void WaylandWindow::Recycle( std::shared_ptr<VlkBase>&& garbage )
{
    m_vkDevice->GetGarbage()->Recycle( m_currentRenderFence.load( std::memory_order_acquire ), std::move( garbage ) );
}

void WaylandWindow::Recycle( std::vector<std::shared_ptr<VlkBase>>&& garbage )
{
    m_vkDevice->GetGarbage()->Recycle( m_currentRenderFence.load( std::memory_order_acquire ), std::move( garbage ) );
}

void WaylandWindow::CreateSwapchain( const VkExtent2D& extent )
{
    const auto scaled = VkExtent2D {
        .width = uint32_t( round( extent.width * m_scale / 120.f ) ),
        .height = uint32_t( round( extent.height * m_scale / 120.f ) )
    };

    auto oldSwapchain = m_swapchain;
    if( m_swapchain ) CleanupSwapchain();
    m_swapchain = std::make_shared<VlkSwapchain>( *m_vkDevice, *m_vkSurface, scaled, m_hdr, oldSwapchain ? *oldSwapchain : VkSwapchainKHR { VK_NULL_HANDLE } );
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

void WaylandWindow::RecalcMaxLuminance()
{
    auto& outputs = m_display.Outputs();
    int l = 0;
    for( auto& wo : m_outputs )
    {
        auto it = std::ranges::find_if( outputs, [wo]( const auto& o ) { return *o == wo; } );
        CheckPanic( it != outputs.end(), "Output not found" );
        l = std::max( l, (*it)->MaxLuminance() );
    }
    m_maxLuminance = l;
    if( m_maxLuminance != m_prevMaxLuminance ) ResumeIfIdle();
}

void WaylandWindow::SurfaceEnter( wl_surface* surface, wl_output* output )
{
    CheckPanic( std::ranges::find( m_outputs, output ) == m_outputs.end(), "Output already exists" );
    m_outputs.emplace_back( output );
    RecalcMaxLuminance();
}

void WaylandWindow::SurfaceLeave( wl_surface* surface, wl_output* output )
{
    auto it = std::ranges::find( m_outputs, output );
    CheckPanic( it != m_outputs.end(), "Output not found" );
    m_outputs.erase( it );
    RecalcMaxLuminance();
}

void WaylandWindow::SurfacePreferredBufferScale( wl_surface* surface, int32_t scale )
{
}

void WaylandWindow::SurfacePreferredBufferTransform( wl_surface* surface, int32_t transform )
{
}

void WaylandWindow::XdgSurfaceConfigure( struct xdg_surface *xdg_surface, uint32_t serial )
{
    xdg_surface_ack_configure( xdg_surface, serial );
    if( !m_vkSurface ) m_vkSurface = std::make_shared<VlkSurface>( m_vkInstance, m_display.Display(), m_surface );
}

void WaylandWindow::XdgToplevelConfigure( struct xdg_toplevel* toplevel, int32_t width, int32_t height, struct wl_array* states )
{
    bool maximized = false;
    bool fullscreen = false;
    for( size_t i=0; i < states->size / sizeof(uint32_t); i++ )
    {
        uint32_t state;
        memcpy( &state, (char*)states->data + i * sizeof( uint32_t ), sizeof( uint32_t ) );
        if( state == XDG_TOPLEVEL_STATE_MAXIMIZED ) maximized = true;
        if( state == XDG_TOPLEVEL_STATE_FULLSCREEN ) fullscreen = true;
    }

    const auto wasMaximized = m_maximized;
    m_maximized = maximized;
    m_fullscreen = fullscreen;

    if( width == 0 || height == 0 )
    {
        if( !wasMaximized ) return;
        m_staged = m_floatingExtent;
    }
    else
    {
        m_staged = {
            .width = uint32_t( width ),
            .height = uint32_t( height )
        };
    }

    ResumeIfIdle();
}

void WaylandWindow::XdgToplevelClose( struct xdg_toplevel* toplevel )
{
    Invoke( OnClose );
}

void WaylandWindow::XdgToplevelConfigureBounds( struct xdg_toplevel* toplevel, int32_t width, int32_t height )
{
    mclog( LogLevel::Debug, "XdgToplevelConfigureBounds: %dx%d", width, height );
    m_bounds = {
        .width = uint32_t( width ),
        .height = uint32_t( height )
    };
}

void WaylandWindow::DecorationConfigure( zxdg_toplevel_decoration_v1* tldec, uint32_t mode )
{
}

void WaylandWindow::FractionalScalePreferredScale( wp_fractional_scale_v1* scale, uint32_t scaleValue )
{
    m_scale = scaleValue;
    ResumeIfIdle();
}

void WaylandWindow::FrameDone( struct wl_callback* cb, uint32_t time )
{
    wl_callback_destroy( cb );
    InvokeRender();
}
