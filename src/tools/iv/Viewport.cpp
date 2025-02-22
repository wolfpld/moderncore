#include <algorithm>
#include <time.h>
#include <tracy/Tracy.hpp>
#include <vulkan/vulkan.h>

#include "Background.hpp"
#include "BusyIndicator.hpp"
#include "ImageProvider.hpp"
#include "ImageView.hpp"
#include "Viewport.hpp"
#include "image/vector/SvgImage.hpp"
#include "util/DataBuffer.hpp"
#include "util/EmbedData.hpp"
#include "util/Invoke.hpp"
#include "util/Panic.hpp"
#include "vulkan/VlkCommandBuffer.hpp"
#include "vulkan/VlkDevice.hpp"
#include "vulkan/VlkInstance.hpp"
#include "vulkan/VlkPhysicalDevice.hpp"
#include "vulkan/ext/PhysDevSel.hpp"
#include "vulkan/ext/Tracy.hpp"
#include "wayland/WaylandDisplay.hpp"
#include "wayland/WaylandWindow.hpp"

#include "data/IconSvg.hpp"

static uint64_t Now()
{
    timespec ts;
    clock_gettime( CLOCK_MONOTONIC, &ts );
    return uint64_t( ts.tv_sec ) * 1000000000 + ts.tv_nsec;
}

Viewport::Viewport( WaylandDisplay& display, VlkInstance& vkInstance, int gpu )
    : m_display( display )
    , m_vkInstance( vkInstance )
    , m_window( std::make_shared<WaylandWindow>( display, vkInstance ) )
    , m_provider( std::make_shared<ImageProvider>() )
{
    ZoneScoped;

    static constexpr WaylandWindow::Listener listener = {
        .OnClose = Method( Close ),
        .OnRender = Method( Render ),
        .OnScale = Method( Scale ),
        .OnResize = Method( Resize )
    };

    Unembed( IconSvg );

    m_window->SetListener( &listener, this );
    m_window->SetAppId( "iv" );
    m_window->SetTitle( "IV" );
    m_window->SetIcon( SvgImage { IconSvg } );
    m_window->Commit();
    m_display.Roundtrip();

    const auto& devices = m_vkInstance.QueryPhysicalDevices();
    CheckPanic( !devices.empty(), "No Vulkan physical devices found" );
    mclog( LogLevel::Info, "Found %d Vulkan physical devices", devices.size() );

    std::shared_ptr<VlkPhysicalDevice> physDevice;
    if( gpu >= 0 )
    {
        CheckPanic( gpu < devices.size(), "Invalid GPU id, must be in range 0 - %d", devices.size() - 1 );
        physDevice = devices[gpu];
    }
    else
    {
        physDevice = PhysDevSel::PickBest( devices, m_window->VkSurface(), PhysDevSel::RequireGraphic );
        CheckPanic( physDevice, "Failed to find suitable Vulkan physical device" );
    }
    mclog( LogLevel::Info, "Selected GPU: %s", physDevice->Properties().deviceName );

    m_device = std::make_shared<VlkDevice>( m_vkInstance, physDevice, VlkDevice::RequireGraphic | VlkDevice::RequirePresent, m_window->VkSurface() );
    m_window->SetDevice( m_device, VkExtent2D { 1650, 900 } );

    const auto format = m_window->GetFormat();
    m_background = std::make_shared<Background>( *m_window, m_device, format, m_scale );
    m_busyIndicator = std::make_shared<BusyIndicator>( *m_window, m_device, format, m_scale );
    m_view = std::make_shared<ImageView>( *m_window, m_device, format );

    m_lastTime = Now();
    m_window->InvokeRender();
}

Viewport::~Viewport()
{
    m_provider->CancelAll();
    m_window->Close();
}

void Viewport::LoadImage( const char* path )
{
    ZoneScoped;

    std::lock_guard lock( m_lock );
    if( !m_isBusy )
    {
        m_isBusy = true;
        m_busyIndicator->ResetTime();
    }
    const auto id = m_provider->LoadImage( path, Method( ImageHandler ), this );

    ZoneTextF( "id %ld", id );
}

void Viewport::Close( WaylandWindow* window )
{
    CheckPanic( window == m_window.get(), "Invalid window" );
    m_display.Stop();
}

bool Viewport::Render( WaylandWindow* window )
{
    ZoneScoped;
    CheckPanic( window == m_window.get(), "Invalid window" );

    const auto now = Now();
    const auto delta = std::min( now - m_lastTime, uint64_t( 1000000000 ) );
    m_lastTime = now;

    FrameMark;
    auto& cmdbuf = window->BeginFrame( true );

    const VkRenderingAttachmentInfo attachmentInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = window->GetImageView(),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE
    };
    const VkRenderingInfo renderingInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = { { 0, 0 }, window->GetExtent() },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachmentInfo
    };

    vkCmdBeginRendering( cmdbuf, &renderingInfo );
    {
        std::lock_guard lock( m_lock );
        ZoneVk( *m_device, cmdbuf, "Viewport", true );
        m_background->Render( cmdbuf, window->GetExtent() );
        if( m_view->HasBitmap() ) m_view->Render( cmdbuf, window->GetExtent() );
        if( m_isBusy )
        {
            m_busyIndicator->Update( delta / 1000000000.f );
            m_busyIndicator->Render( cmdbuf, window->GetExtent() );
        }
    }
    vkCmdEndRendering( cmdbuf );
    window->EndFrame();

    return true;
}

void Viewport::Scale( WaylandWindow* window, uint32_t scale )
{
    ZoneScoped;
    ZoneValue( scale );
    CheckPanic( window == m_window.get(), "Invalid window" );

    mclog( LogLevel::Info, "Preferred window scale: %g", scale / 120.f );

    if( m_background )
    {
        m_background->SetScale( scale / 120.f );
        m_busyIndicator->SetScale( scale / 120.f );
    }
    else
    {
        m_scale = scale / 120.f;
    }
}

void Viewport::Resize( WaylandWindow* window, uint32_t width, uint32_t height )
{
    ZoneScoped;
    ZoneTextF( "width %u, height %u", width, height );

    CheckPanic( window == m_window.get(), "Invalid window" );
    window->Resize( width, height );
}

void Viewport::ImageHandler( int64_t id, int result, std::shared_ptr<Bitmap> bitmap )
{
    ZoneScoped;
    ZoneTextF( "id %ld, result %d", id, result );

    m_view->SetBitmap( bitmap );

    std::lock_guard lock( m_lock );
    m_isBusy = false;
}
