#include <algorithm>
#include <linux/input-event-codes.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <tracy/Tracy.hpp>
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#include "Background.hpp"
#include "BusyIndicator.hpp"
#include "ImageView.hpp"
#include "Viewport.hpp"
#include "image/vector/SvgImage.hpp"
#include "util/Bitmap.hpp"
#include "util/BitmapHdr.hpp"
#include "util/Config.hpp"
#include "util/DataBuffer.hpp"
#include "util/EmbedData.hpp"
#include "util/Filesystem.hpp"
#include "util/Invoke.hpp"
#include "util/MemoryBuffer.hpp"
#include "util/Panic.hpp"
#include "util/TaskDispatch.hpp"
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
    , m_td( std::make_unique<TaskDispatch>( std::thread::hardware_concurrency() - 1, "Worker" ) )
    , m_window( std::make_shared<WaylandWindow>( display, vkInstance ) )
    , m_provider( std::make_shared<ImageProvider>( *m_td ) )
{
    ZoneScoped;

    static constexpr WaylandWindow::Listener listener = {
        .OnClose = Method( Close ),
        .OnRender = Method( Render ),
        .OnScale = Method( Scale ),
        .OnResize = Method( Resize ),
        .OnFormatChange = Method( FormatChange ),
        .OnClipboard = Method( Clipboard ),
        .OnDrag = Method( Drag ),
        .OnDrop = Method( Drop ),
        .OnKey = Method( Key ),
        .OnMouseEnter = Method( MouseEnter ),
        .OnMouseLeave = Method( MouseLeave ),
        .OnMouseMove = Method( MouseMove ),
        .OnMouseButton = Method( MouseButton )
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

    Config cfg( "iv.ini" );
    const auto width = cfg.Get( "Window", "Width", 1280 );
    const auto height = cfg.Get( "Window", "Height", 720 );

    m_device = std::make_shared<VlkDevice>( m_vkInstance, physDevice, VlkDevice::RequireGraphic | VlkDevice::RequirePresent, m_window->VkSurface() );
    PrintQueueConfig( *m_device );
    m_window->SetDevice( m_device );
    m_window->ResizeNoScale( width, height );

    if( m_window->HdrCapable() ) mclog( LogLevel::Info, "HDR capable" );

    const auto format = m_window->GetFormat();
    const auto scale = m_window->GetScale() / 120.f;
    m_background = std::make_shared<Background>( *m_window, m_device, format );
    m_busyIndicator = std::make_shared<BusyIndicator>( *m_window, m_device, format, scale );
    m_view = std::make_shared<ImageView>( *m_window, m_device, format, m_window->GetSize(), scale );

    const char* token = getenv( "XDG_ACTIVATION_TOKEN" );
    if( token )
    {
        m_window->Activate( token );
        unsetenv( "XDG_ACTIVATION_TOKEN" );
    }

    m_lastTime = Now();
    m_window->InvokeRender();
}

Viewport::~Viewport()
{
    const auto winSize = m_window->GetSizeNoScale();
    const auto maximized = m_window->Maximized();

    m_window->Close();
    m_provider->CancelAll();
    m_provider.reset();

    const auto configPath = Config::GetPath();
    if( CreateDirectories( configPath ) )
    {
        FILE* f = fopen( ( configPath + "iv.ini" ).c_str(), "w" );
        if( f )
        {
            fprintf( f, "[Window]\n" );
            fprintf( f, "Width = %u\n", winSize.width );
            fprintf( f, "Height = %u\n", winSize.height );
            fprintf( f, "Maximized = %d\n", maximized );
            fclose( f );
        }
    }
}

void Viewport::LoadImage( const char* path )
{
    ZoneScoped;
    const auto id = m_provider->LoadImage( path, m_window->HdrCapable(), Method( ImageHandler ), this );
    ZoneTextF( "id %ld", id );
    SetBusy( id );
}

void Viewport::LoadImage( std::unique_ptr<DataBuffer>&& data )
{
    ZoneScoped;
    const auto id = m_provider->LoadImage( std::move( data ), m_window->HdrCapable(), Method( ImageHandler ), this );
    ZoneTextF( "id %ld", id );
    SetBusy( id );
}

void Viewport::LoadImage( int fd, int flags )
{
    ZoneScoped;
    const auto id = m_provider->LoadImage( fd, m_window->HdrCapable(), Method( ImageHandler ), this, flags );
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
        WantRender();
    }
}

void Viewport::Update( float delta )
{
    std::lock_guard lock( m_lock );
    if( m_isBusy )
    {
        m_busyIndicator->Update( delta );
        m_render = true;
    }
}

void Viewport::WantRender()
{
    if( m_render ) return;
    m_render = true;
    m_window->ResumeIfIdle();
}

void Viewport::Close()
{
    m_display.Stop();
}

bool Viewport::Render()
{
    ZoneScoped;

    const auto now = Now();
    const auto delta = std::min( now - m_lastTime, uint64_t( 1000000000 ) ) / 1000000000.f;
    m_lastTime = now;

    m_window->Update();
    Update( delta );

    if( !m_render ) return false;
    m_render = false;

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
        .renderArea = { { 0, 0 }, m_window->GetSize() },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachmentInfo
    };

    vkCmdBeginRendering( cmdbuf, &renderingInfo );
    {
        std::lock_guard lock( m_lock );
        ZoneVk( *m_device, cmdbuf, "Viewport", true );
        m_background->Render( cmdbuf, m_window->GetSize() );
        if( m_view->HasBitmap() ) m_view->Render( cmdbuf, m_window->GetSize() );
        if( m_isBusy )
        {
            m_busyIndicator->Render( cmdbuf, m_window->GetSize() );
        }
    }
    vkCmdEndRendering( cmdbuf );
    m_window->EndFrame();

    return true;
}

void Viewport::Scale( uint32_t width, uint32_t height, uint32_t scale )
{
    ZoneScoped;
    ZoneTextF( "scale %u, width %u, height %u", scale, width, height );

    mclog( LogLevel::Info, "Preferred window scale: %g, size: %ux%u", scale / 120.f, width, height );

    m_busyIndicator->SetScale( scale / 120.f );
    m_view->SetScale( scale / 120.f, m_window->GetSize() );

    std::lock_guard lock( m_lock );
    m_render = true;
}

void Viewport::Resize( uint32_t width, uint32_t height )
{
    ZoneScoped;
    ZoneTextF( "width %u, height %u", width, height );

    m_view->Resize( m_window->GetSize() );

    std::lock_guard lock( m_lock );
    m_render = true;
}

void Viewport::FormatChange( VkFormat format )
{
    ZoneScoped;
    ZoneTextF( "%s", string_VkFormat( format ) );

    m_background->FormatChange( format );
    m_busyIndicator->FormatChange( format );
    m_view->FormatChange( format );

    std::lock_guard lock( m_lock );
    m_render = true;
}

void Viewport::Clipboard( const unordered_flat_set<std::string>& mimeTypes )
{
    m_clipboardOffer = mimeTypes;
}

void Viewport::Drag( const unordered_flat_set<std::string>& mimeTypes )
{
    if( mimeTypes.empty() ) return;

    static constexpr std::array mimes = {
        "image/png",
        "text/uri-list"
    };

    for( const auto& mime : mimes )
    {
        if( mimeTypes.contains( mime ) )
        {
            m_window->AcceptDndMime( mime );
            return;
        }
    }

    m_window->AcceptDndMime( nullptr );
}

void Viewport::Drop( int fd, const char* mime )
{
    if( strcmp( mime, "text/uri-list" ) == 0 )
    {
        auto fn = MemoryBuffer( fd ).AsString();
        m_window->FinishDnd( fd );
        ProcessUriList( std::move( fn ) );
    }
    else if( strcmp( mime, "image/png" ) == 0 )
    {
        LoadImage( fd, fd + 1 );
    }
    else
    {
        Panic( "Unsupported MIME type: %s", mime );
    }
}

void Viewport::Key( const char* key, int mods )
{
    ZoneScoped;
    ZoneText( key, strlen( key ) );

    if( mods & CtrlBit && strcmp( key, "v" ) == 0 )
    {
        PasteClipboard();
    }
    else if( mods == 0 && strcmp( key, "f" ) == 0 )
    {
        m_view->FitToExtent( m_window->GetSize() );
        std::lock_guard lock( m_lock );
        WantRender();
    }
    else if( mods == ShiftBit && strcmp( key, "F" ) == 0 )
    {
        if( !m_view->HasBitmap() ) return;

        std::lock_guard lock( *m_window );
        if( m_window->Maximized() )
        {
            m_view->FitToExtent( m_window->GetSize() );
            std::lock_guard lock( m_lock );
            WantRender();
        }
        else
        {
            const auto size = m_view->GetBitmapExtent();
            const auto bounds = m_window->GetBounds();
            if( bounds.width != 0 && bounds.height != 0 )
            {
                uint32_t w, h;
                if( bounds.width >= size.width && bounds.height >= size.height )
                {
                    w = size.width;
                    h = size.height;
                }
                else
                {
                    const auto scale = std::min( float( bounds.width ) / size.width, float( bounds.height ) / size.height );
                    w = size.width * scale;
                    h = size.height * scale;
                }

                // Don't let the window get too small. 150 px is the minimum window size KDE allows.
                const auto dpi = m_window->GetScale();
                const auto minSize = 150 * dpi / 120;
                w = std::max( w, minSize );
                h = std::max( h, minSize );

                m_window->Resize( w, h, true );
                m_view->FitToExtent( VkExtent2D( w, h ) );
            }
            else
            {
                m_view->FitToExtent( m_window->GetSize() );
                std::lock_guard lock( m_lock );
                WantRender();
            }
        }
    }
    else if( mods == 0 && strcmp( key, "1" ) == 0 )
    {
        if( !m_view->HasBitmap() ) return;
        m_view->FitPixelPerfect( m_window->GetSize() );
        std::lock_guard lock( m_lock );
        WantRender();
    }
}

void Viewport::MouseEnter( float x, float y )
{
    m_mousePos = { x, y };
}

void Viewport::MouseLeave()
{
}

void Viewport::MouseMove( float x, float y )
{
    if( m_dragActive )
    {
        m_view->Pan( { x - m_mousePos.x, y - m_mousePos.y } );
        std::lock_guard lock( m_lock );
        WantRender();
    }
    m_mousePos = { x, y };
}

void Viewport::MouseButton( uint32_t button, bool pressed )
{
    if( button == BTN_RIGHT )
    {
        m_dragActive = pressed;
        m_window->SetCursor( m_dragActive ? WaylandCursor::Grabbing : WaylandCursor::Default );
        m_window->ResumeIfIdle();
    }
}

void Viewport::ImageHandler( int64_t id, ImageProvider::Result result, int flags, const ImageProvider::ReturnData& data )
{
    ZoneScoped;
    ZoneTextF( "id %ld, result %d", id, result );

    if( flags != 0 ) m_window->FinishDnd( flags - 1 );

    if( result == ImageProvider::Result::Success )
    {
        uint32_t width, height;
        if( data.bitmap )
        {
            m_view->SetBitmap( data.bitmap, *m_td );
            width = data.bitmap->Width();
            height = data.bitmap->Height();
            m_window->EnableHdr( false );
        }
        else
        {
            m_view->SetBitmap( data.bitmapHdr, *m_td );
            width = data.bitmapHdr->Width();
            height = data.bitmapHdr->Height();
            m_window->EnableHdr( true );
        }

        m_lock.lock();
        WantRender();
    }
    else
    {
        m_lock.lock();
    }

    if( m_currentJob == id )
    {
        m_currentJob = -1;
        m_isBusy = false;
        m_window->SetCursor( WaylandCursor::Default );
    }
    m_lock.unlock();
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
