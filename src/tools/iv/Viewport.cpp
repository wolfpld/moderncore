#include <algorithm>
#include <dirent.h>
#include <format>
#include <linux/input-event-codes.h>
#include <numbers>
#include <stdlib.h>
#include <string_view>
#include <sys/stat.h>
#include <time.h>
#include <tracy/Tracy.hpp>
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#include "Background.hpp"
#include "BusyIndicator.hpp"
#include "ImageView.hpp"
#include "Viewport.hpp"
#include "image/ImageLoader.hpp"
#include "image/vector/SvgImage.hpp"
#include "util/Bitmap.hpp"
#include "util/BitmapHdr.hpp"
#include "util/Config.hpp"
#include "util/DataBuffer.hpp"
#include "util/EmbedData.hpp"
#include "util/Filesystem.hpp"
#include "util/Home.hpp"
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
#include "wayland/WaylandScroll.hpp"
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
        .OnKeyEvent = Method( KeyEvent ),
        .OnMouseEnter = Method( MouseEnter ),
        .OnMouseLeave = Method( MouseLeave ),
        .OnMouseMove = Method( MouseMove ),
        .OnMouseButton = Method( MouseButton ),
        .OnScroll = Method( Scroll )
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
    const auto winSize = m_window->GetSizeFloating();
    const auto maximized = m_window->IsMaximized();

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

void Viewport::LoadImage( const char* path, bool scanDirectory )
{
    ZoneScoped;
    std::lock_guard lock( m_lock );
    const auto id = m_provider->LoadImage( path, m_window->HdrCapable(), Method( ImageHandler ), this );
    ZoneTextF( "id %ld", id );
    SetBusy( id );

    if( scanDirectory )
    {
        auto pos = std::string_view( path ).find_last_of( '/' );
        if( pos != std::string_view::npos )
        {
            const auto dir = std::string( path, 0, pos );
            mclog( LogLevel::Info, "Scanning directory %s", dir.c_str() );
            auto files = FindLoadableImages( ListDirectory( dir ) );
            if( files.empty() ) return;
            mclog( LogLevel::Info, "Found %zu files", files.size() );
            SetFileList( std::move( files ), path );
        }
    }
}

void Viewport::LoadImage( std::unique_ptr<DataBuffer>&& data )
{
    ZoneScoped;
    std::lock_guard lock( m_lock );
    const auto id = m_provider->LoadImage( std::move( data ), m_window->HdrCapable(), Method( ImageHandler ), this );
    ZoneTextF( "id %ld", id );
    SetBusy( id );
}

void Viewport::LoadImage( int fd, const char* origin, int dndFd )
{
    ZoneScoped;
    std::lock_guard lock( m_lock );
    const auto id = m_provider->LoadImage( fd, m_window->HdrCapable(), Method( ImageHandler ), this, origin, { .dndFd = dndFd } );
    ZoneTextF( "id %ld", id );
    SetBusy( id );
}

void Viewport::LoadImage( const std::vector<std::string>& paths )
{
    ZoneScoped;
    CheckPanic( !paths.empty(), "No files to load" );

    auto files = FindLoadableImages( paths );
    if( files.empty() )
    {
        mclog( LogLevel::Error, "No valid files to load" );
        return;
    }

    if( files.size() == 1 )
    {
        LoadImage( files[0].c_str(), true );
    }
    else
    {
        m_fileList = std::move( files );
        m_fileIndex = 0;
        m_origin = m_fileList[0];
        LoadImage( m_fileList[0].c_str(), false );
    }
}

void Viewport::SetBusy( int64_t job )
{
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

    if( m_view->HasBitmap() )
    {
        const auto imgScale = m_view->GetImgScale();
        if( imgScale != m_viewScale )
        {
            m_viewScale = imgScale;
            m_updateTitle = true;
        }
    }
    if( m_updateTitle )
    {
        m_updateTitle = false;
        const auto extent = m_view->GetBitmapExtent();
        if( m_fileList.size() > 1 )
        {
            m_window->SetTitle( std::format( "{} [{}/{}] - {}×{} - {:.2f}% — IV", m_origin, m_fileIndex + 1, m_fileList.size(), extent.width, extent.height, m_viewScale * 100 ).c_str() );
        }
        else
        {
            m_window->SetTitle( std::format( "{} - {}×{} - {:.2f}% — IV", m_origin, extent.width, extent.height, m_viewScale * 100 ).c_str() );
        }
    }
}

void Viewport::WantRender()
{
    if( m_render ) return;
    m_lastTime = Now();
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

    m_lock.lock();
    const auto now = Now();
    const auto delta = std::min( now - m_lastTime, uint64_t( 1000000000 ) ) / 1000000000.f;
    m_lastTime = now;
    m_lock.unlock();

    m_window->Update();

    std::lock_guard lock( *m_view );
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

    m_view->lock();
    m_view->SetScale( scale / 120.f, m_window->GetSize() );
    m_view->unlock();

    std::lock_guard lock( m_lock );
    m_render = true;
}

void Viewport::Resize( uint32_t width, uint32_t height )
{
    ZoneScoped;
    ZoneTextF( "width %u, height %u", width, height );

    m_view->lock();
    m_view->Resize( m_window->GetSize() );
    m_view->unlock();

    std::lock_guard lock( m_lock );
    m_render = true;
}

void Viewport::FormatChange( VkFormat format )
{
    ZoneScoped;
    ZoneTextF( "%s", string_VkFormat( format ) );

    m_background->FormatChange( format );
    m_busyIndicator->FormatChange( format );

    m_view->lock();
    m_view->FormatChange( format );
    m_view->unlock();

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

    auto it = mimeTypes.find( "text/uri-list" );
    if( it != mimeTypes.end() )
    {
        const auto uriList = ProcessUriList( MemoryBuffer( m_window->GetDnd( "text/uri-list" ) ).AsString() );
        const auto files = FindLoadableImages( FindValidFiles( uriList ) );
        if( !files.empty() )
        {
            m_window->AcceptDndMime( "text/uri-list" );
            return;
        }
        else if( !uriList.empty() )
        {
            m_loadOrigin = uriList[0];
        }
    }

    it = mimeTypes.find( "image/png" );
    if( it != mimeTypes.end() )
    {
        m_window->AcceptDndMime( "image/png" );
        return;
    }

    m_window->AcceptDndMime( nullptr );
}

void Viewport::Drop( int fd, const char* mime )
{
    if( strcmp( mime, "text/uri-list" ) == 0 )
    {
        auto fn = MemoryBuffer( fd ).AsString();
        m_window->FinishDnd( fd );
        const auto uriList = ProcessUriList( std::move( fn ) );
        auto files = FindLoadableImages( FindValidFiles( uriList ) );
        if( !files.empty() )
        {
            if( files.size() == 1 )
            {
                LoadImage( files[0].c_str(), true );
            }
            else
            {
                LoadImage( files[0].c_str(), false );
                SetFileList( std::move( files ), files[0] );
            }
        }
    }
    else if( strcmp( mime, "image/png" ) == 0 )
    {
        LoadImage( fd, m_loadOrigin.c_str(), fd + 1 );
        m_fileList.clear();
    }
    else
    {
        Panic( "Unsupported MIME type: %s", mime );
    }

    m_loadOrigin.clear();
}

void Viewport::KeyEvent( uint32_t key, int mods, bool pressed )
{
    if( !pressed ) return;

    ZoneScoped;
    ZoneValue( key );

    if( mods & CtrlBit && key == KEY_V )
    {
        PasteClipboard();
    }
    else if( key == KEY_F )
    {
        std::lock_guard lock( *m_view );
        if( !m_view->HasBitmap() ) return;
        if( mods == 0 )
        {
            m_view->FitToExtent( m_window->GetSize() );
            std::lock_guard lock( m_lock );
            WantRender();
        }
        else if( mods == CtrlBit )
        {
            m_view->FitToWindow( m_window->GetSize() );
            std::lock_guard lock( m_lock );
            WantRender();
        }
        else if( mods == ShiftBit )
        {
            std::lock_guard lock( *m_window );
            if( m_window->IsMaximized() || m_window->IsFullscreen() )
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
    }
    else if( mods == 0 && key >= KEY_1 && key <= KEY_4 )
    {
        std::unique_lock viewLock( *m_view );
        if( !m_view->HasBitmap() ) return;
        m_view->FitPixelPerfect( m_window->GetSize(), 1 << ( key - KEY_1 ), m_mouseFocus ? &m_mousePos : nullptr );
        viewLock.unlock();

        std::lock_guard lock( m_lock );
        WantRender();
    }
    else if( mods == 0 && ( key == KEY_F11 || ( key == KEY_ESC && m_window->IsFullscreen() ) ) )
    {
        m_window->Fullscreen( !m_window->IsFullscreen() );
        std::lock_guard lock( m_lock );
        WantRender();
    }
    else if( mods == 0 && key == KEY_ESC )
    {
        Close();
    }
    else if( mods == 0 && key == KEY_RIGHT )
    {
        if( m_fileList.size() > 1 )
        {
            m_fileIndex = ( m_fileIndex + 1 ) % m_fileList.size();
            LoadImage( m_fileList[m_fileIndex].c_str(), false );
        }
    }
    else if( mods == 0 && key == KEY_LEFT )
    {
        if( m_fileList.size() > 1 )
        {
            m_fileIndex = ( m_fileIndex + m_fileList.size() - 1 ) % m_fileList.size();
            LoadImage( m_fileList[m_fileIndex].c_str(), false );
        }
    }
}

void Viewport::MouseEnter( float x, float y )
{
    m_mousePos = { x, y };
    m_mouseFocus = true;
}

void Viewport::MouseLeave()
{
    m_mouseFocus = false;
}

void Viewport::MouseMove( float x, float y )
{
    if( m_dragActive )
    {
        m_view->lock();
        m_view->Pan( { x - m_mousePos.x, y - m_mousePos.y } );
        m_view->unlock();

        std::lock_guard lock( m_lock );
        WantRender();
    }
    m_mousePos = { x, y };
}

void Viewport::MouseButton( uint32_t button, bool pressed )
{
    if( button == BTN_RIGHT )
    {
        std::lock_guard lock( *m_view );
        if( m_view->HasBitmap() )
        {
            m_dragActive = pressed;
            m_window->SetCursor( m_dragActive ? WaylandCursor::Grabbing : WaylandCursor::Default );
            m_window->ResumeIfIdle();
        }
    }
}

void Viewport::Scroll( const WaylandScroll& scroll )
{
    if( scroll.delta.y != 0 )
    {
        std::unique_lock viewLock( *m_view );
        if( m_view->HasBitmap() )
        {
            float factor;
            const auto delta = -scroll.delta.y;
            if( scroll.source == WaylandScroll::Source::Wheel )
            {
                factor = delta / 15.f * std::numbers::sqrt2_v<float>;
                if( delta < 0 ) factor = -1.f / factor;
            }
            else
            {
                factor = 1 + delta * 0.01f;
            }
            m_view->Zoom( m_mousePos, factor );
            viewLock.unlock();

            std::lock_guard lock( m_lock );
            WantRender();
        }
    }
}

void Viewport::ImageHandler( int64_t id, ImageProvider::Result result, const ImageProvider::ReturnData& data )
{
    ZoneScoped;
    ZoneTextF( "id %ld, result %d", id, result );

    if( data.flags.dndFd != 0 ) m_window->FinishDnd( data.flags.dndFd - 1 );

    if( result == ImageProvider::Result::Success )
    {
        uint32_t width, height;
        if( data.bitmap )
        {
            // must not lock m_view here
            m_view->SetBitmap( data.bitmap, *m_td );
            width = data.bitmap->Width();
            height = data.bitmap->Height();
            m_window->EnableHdr( false );
        }
        else
        {
            // must not lock m_view here
            m_view->SetBitmap( data.bitmapHdr, *m_td );
            width = data.bitmapHdr->Width();
            height = data.bitmapHdr->Height();
            m_window->EnableHdr( true );
        }

        m_lock.lock();
        if( data.origin.empty() )
        {
            m_origin = "Untitled";
        }
        else
        {
            m_origin = data.origin.substr( data.origin.find_last_of( '/' ) + 1 );
            if( m_origin.empty() ) m_origin = "Untitled";
        }
        m_updateTitle = true;
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
        WantRender();
    }
    m_lock.unlock();
}

void Viewport::PasteClipboard()
{
    ZoneScoped;
    mclog( LogLevel::Info, "Clipboard paste" );

    std::string loadOrigin;
    if( m_clipboardOffer.contains( "text/uri-list" ) )
    {
        const auto uriList = ProcessUriList( MemoryBuffer( m_window->GetClipboard( "text/uri-list" ) ).AsString() );
        auto files = FindLoadableImages( FindValidFiles( uriList ) );
        if( !files.empty() )
        {
            if( files.size() == 1 )
            {
                LoadImage( files[0].c_str(), true );
            }
            else
            {
                LoadImage( files[0].c_str(), false );
                SetFileList( std::move( files ), files[0] );
            }
            return;
        }
        else if( !uriList.empty() )
        {
            loadOrigin = uriList[0];
        }
    }
    if( m_clipboardOffer.contains( "image/png" ) )
    {
        LoadImage( m_window->GetClipboard( "image/png" ), loadOrigin.c_str() );
    }
}

std::vector<std::string> Viewport::ProcessUriList( std::string uriList )
{
    std::vector<std::string> ret;
    while( !uriList.empty() )
    {
        auto pos = uriList.find_first_of( "\r\n" );
        if( pos == 0 )
        {
            uriList.erase( 0, 1 );
            continue;
        }
        if( pos == std::string::npos ) pos = uriList.size();
        auto uri = uriList.substr( 0, pos );
        uriList.erase( 0, pos + 1 );
        UrlDecode( uri );
        ret.emplace_back( std::move( uri ) );
    }
    return ret;
}

std::vector<std::string> Viewport::FindValidFiles( const std::vector<std::string>& uriList )
{
    std::vector<std::string> ret;
    for( const auto& uri : uriList )
    {
        if( uri.starts_with( "file://" ) )
        {
            auto path = uri.substr( 7 );
            struct stat st;
            if( stat( path.c_str(), &st ) == 0 && S_ISREG( st.st_mode ) ) 
            {
                if( path[0] == '~' )
                {
                    ret.emplace_back( ExpandHome( path.c_str() ) );
                }
                else
                {
                    ret.emplace_back( std::move( path ) );
                }
            }
        }
    }
    return ret;
}

std::vector<std::string> Viewport::FindLoadableImages( const std::vector<std::string>& fileList )
{
    const auto cpus = std::thread::hardware_concurrency();
    std::vector<std::vector<std::string>> tmp( cpus );
    const auto batchSize = ( fileList.size() + cpus - 1 ) / cpus;
    size_t start = 0;
    int n;
    for( n=0; n<cpus; n++ )
    {
        if( start >= fileList.size() ) break;
        tmp[n].reserve( batchSize );
        m_td->Queue( [&fileList, &tmp, n, batchSize, start]() {
            const auto end = std::min( start + batchSize, fileList.size() );
            for( size_t i = start; i < end; i++ )
            {
                auto loader = GetImageLoader( fileList[i].c_str(), ToneMap::Operator::PbrNeutral );
                if( loader ) tmp[n].emplace_back( fileList[i] );
            }
        } );
        start += batchSize;
    }
    m_td->Sync();

    std::vector<std::string> ret;
    for( int i=0; i<n; i++ )
    {
        auto& v = tmp[i];
        ret.insert( ret.end(), v.begin(), v.end() );
    }
    return ret;
}

void Viewport::SetFileList( std::vector<std::string>&& fileList, const std::string& origin )
{
    m_fileList = fileList;
    auto it = std::ranges::find( m_fileList, origin );
    CheckPanic( it != m_fileList.end(), "Origin not found in file list" );
    m_fileIndex = std::distance( m_fileList.begin(), it );
    mclog( LogLevel::Info, "File list: %zu files, current: %zu", m_fileList.size(), m_fileIndex );
}

std::vector<std::string> Viewport::ListDirectory( const std::string& path )
{
    std::vector<std::string> ret;
    DIR* dir = opendir( path.c_str() );
    if( !dir ) return ret;

    struct dirent* entry;
    while( ( entry = readdir( dir ) ) )
    {
        if( entry->d_type == DT_REG )
        {
            ret.emplace_back( path + "/" + entry->d_name );
        }
        else if( entry->d_type == DT_LNK )
        {
            auto srcPath = path + "/" + entry->d_name;
            char link[PATH_MAX];
            if( readlink( srcPath.c_str(), link, sizeof( link ) ) != -1 )
            {
                struct stat st;
                if( stat( link, &st ) == 0 && S_ISREG( st.st_mode ) )
                {
                    ret.emplace_back( std::move( srcPath ) );
                }
            }
        }
    }
    closedir( dir );
    std::ranges::sort( ret );
    return ret;
}
