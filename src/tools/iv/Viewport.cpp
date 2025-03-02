#include <algorithm>
#include <sys/stat.h>
#include <time.h>
#include <tracy/Tracy.hpp>
#include <vulkan/vulkan.h>

#include "Background.hpp"
#include "BusyIndicator.hpp"
#include "ImageView.hpp"
#include "Viewport.hpp"
#include "image/vector/SvgImage.hpp"
#include "util/DataBuffer.hpp"
#include "util/EmbedData.hpp"
#include "util/Invoke.hpp"
#include "util/MemoryBuffer.hpp"
#include "util/Panic.hpp"
#include "util/Url.hpp"
#include "vulkan/VlkCommandBuffer.hpp"
#include "vulkan/VlkDevice.hpp"
#include "vulkan/VlkInstance.hpp"
#include "vulkan/VlkPhysicalDevice.hpp"
#include "vulkan/ext/DeviceInfo.hpp"
#include "vulkan/ext/PhysDevSel.hpp"
#include "vulkan/ext/Tracy.hpp"
#include "wayland/WaylandCursor.hpp"
#include "wayland/WaylandDisplay.hpp"
#include "wayland/WaylandKeys.hpp"
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
        .OnResize = Method( Resize ),
        .OnClipboard = Method( Clipboard ),
        .OnKey = Method( Key )
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
    PrintQueueConfig( *m_device );
    m_window->SetDevice( m_device, VkExtent2D { 1650, 900 } );

    const auto format = m_window->GetFormat();
    const auto scale = m_window->GetScale() / 120.f;
    m_background = std::make_shared<Background>( *m_window, m_device, format );
    m_busyIndicator = std::make_shared<BusyIndicator>( *m_window, m_device, format, scale );
    m_view = std::make_shared<ImageView>( *m_window, m_device, format, m_window->GetExtent(), scale );

    m_lastTime = Now();
    m_window->InvokeRender();
}

Viewport::~Viewport()
{
    m_window->Close();
    m_provider->CancelAll();
    m_provider.reset();
}

void Viewport::LoadImage( const char* path )
{
    ZoneScoped;
    const auto id = m_provider->LoadImage( path, Method( ImageHandler ), this );
    ZoneTextF( "id %ld", id );
    SetBusy( id );
}

void Viewport::LoadImage( std::unique_ptr<DataBuffer>&& buffer )
{
    ZoneScoped;
    const auto id = m_provider->LoadImage( std::move( buffer ), Method( ImageHandler ), this );
    ZoneTextF( "id %ld", id );
    SetBusy( id );
}

void Viewport::LoadImage( int fd, int flags )
{
    ZoneScoped;
    const auto id = m_provider->LoadImage( fd, Method( ImageHandler ), this, flags );
    ZoneTextF( "id %ld", id );
    SetBusy( id );
}

void Viewport::SetBusy( int64_t job )
{
    std::lock_guard lock( m_lock );

    if( m_currentJob != -1 ) m_provider->Cancel( m_currentJob );
    m_currentJob = job;

    if( !m_isBusy )
    {
        m_isBusy = true;
        m_busyIndicator->ResetTime();
        m_window->SetCursor( WaylandCursor::Wait );
    }
}

void Viewport::Close()
{
    m_display.Stop();
}

bool Viewport::Render()
{
    ZoneScoped;

    const auto now = Now();
    const auto delta = std::min( now - m_lastTime, uint64_t( 1000000000 ) );
    m_lastTime = now;

    FrameMark;
    auto& cmdbuf = m_window->BeginFrame();

    const VkRenderingAttachmentInfo attachmentInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = m_window->GetImageView(),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE
    };
    const VkRenderingInfo renderingInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = { { 0, 0 }, m_window->GetExtent() },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachmentInfo
    };

    vkCmdBeginRendering( cmdbuf, &renderingInfo );
    {
        std::lock_guard lock( m_lock );
        ZoneVk( *m_device, cmdbuf, "Viewport", true );
        m_background->Render( cmdbuf, m_window->GetExtent() );
        if( m_view->HasBitmap() ) m_view->Render( cmdbuf, m_window->GetExtent() );
        if( m_isBusy )
        {
            m_busyIndicator->Update( delta / 1000000000.f );
            m_busyIndicator->Render( cmdbuf, m_window->GetExtent() );
        }
    }
    vkCmdEndRendering( cmdbuf );
    m_window->EndFrame();

    return true;
}

void Viewport::Scale( uint32_t scale )
{
    ZoneScoped;
    ZoneValue( scale );

    mclog( LogLevel::Info, "Preferred window scale: %g", scale / 120.f );

    m_busyIndicator->SetScale( scale / 120.f );
    m_view->SetScale( scale / 120.f );
}

void Viewport::Resize( uint32_t width, uint32_t height )
{
    ZoneScoped;
    ZoneTextF( "width %u, height %u", width, height );

    m_view->Resize( m_window->GetExtent() );
}

void Viewport::Clipboard( const unordered_flat_set<std::string>& mimeTypes )
{
    m_clipboardOffer = mimeTypes;
}

void Viewport::Key( const char* key, int mods )
{
    ZoneScoped;
    ZoneText( key, strlen( key ) );

    if( mods & CtrlBit && strcmp( key, "v" ) == 0 ) PasteClipboard();
}

void Viewport::ImageHandler( int64_t id, ImageProvider::Result result, int flags, std::shared_ptr<Bitmap> bitmap )
{
    ZoneScoped;
    ZoneTextF( "id %ld, result %d", id, result );

    if( result == ImageProvider::Result::Success ) m_view->SetBitmap( bitmap );

    std::lock_guard lock( m_lock );
    if( m_currentJob == id )
    {
        m_currentJob = -1;
        m_isBusy = false;
        m_window->SetCursor( WaylandCursor::Default );
    }
}

void Viewport::PasteClipboard()
{
    ZoneScoped;
    mclog( LogLevel::Info, "Clipboard paste" );

    if( m_clipboardOffer.contains( "text/uri-list" ) )
    {
        if( ProcessUriList( MemoryBuffer( m_window->GetClipboard( "text/uri-list" ) ).AsString() ) ) return;
    }
    if( m_clipboardOffer.contains( "image/png" ) )
    {
        LoadImage( m_window->GetClipboard( "image/png" ) );
    }
}

bool Viewport::ProcessUriList( std::string uriList )
{
    if( uriList.starts_with( "file://" ) )
    {
        uriList.erase( 0, 7 );
        auto pos = uriList.find_first_of( "\r\n" );
        if( pos != std::string::npos ) uriList.resize( pos );
        UrlDecode( uriList );
        struct stat st;
        if( stat( uriList.c_str(), &st ) == 0 && S_ISREG( st.st_mode ) )
        {
            LoadImage( uriList.c_str() );
            return true;
        }
    }
    return false;
}
