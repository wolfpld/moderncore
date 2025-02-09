#include <format>
#include <inttypes.h>
#include <tracy/Tracy.hpp>

#include "BackendWayland.hpp"
#include "backend/GpuDevice.hpp"
#include "server/Server.hpp"
#include "util/Config.hpp"
#include "util/Invoke.hpp"
#include "util/Panic.hpp"
#include "util/Tracy.hpp"
#include "vulkan/VlkCommandBuffer.hpp"
#include "vulkan/VlkDevice.hpp"
#include "vulkan/VlkInstance.hpp"
#include "vulkan/ext/PhysDevSel.hpp"
#include "wayland/WaylandDisplay.hpp"
#include "wayland/WaylandWindow.hpp"

BackendWayland::BackendWayland()
    : m_dpy( std::make_unique<WaylandDisplay>() )
{
    m_dpy->Connect();
}

BackendWayland::~BackendWayland()
{
    m_windows.clear();
}

void BackendWayland::VulkanInit()
{
    ZoneScoped;

    auto& vkInstance = Server::Instance().VkInstance();
    const auto& devices = vkInstance.QueryPhysicalDevices();
    CheckPanic( !devices.empty(), "No physical devices found" );
    mclog( LogLevel::Info, "Found %d physical devices", devices.size() );

    for( int idx = 0; const auto& dev : devices )
    {
        mclog( LogLevel::Info, "  %d: %s", idx++, dev->Properties().deviceName );
    }

    m_gpus.reserve( devices.size() );
    for( auto& dev : devices )
    {
        try
        {
            m_gpus.emplace_back( std::make_shared<GpuDevice>( vkInstance, dev ) );
        }
        catch( const std::exception& e )
        {
            mclog( LogLevel::Fatal, "Failed to initialize GPU: %s", e.what() );
        }
    }

    Config config( "backend-wayland.ini" );
    if( config )
    {
        bool windowAdded = false;
        int idx = 0;
        while( true )
        {
            const auto section = std::format( "Window{}", idx );
            int physDev;
            uint32_t width, height;
            if( !config.GetOpt( section.c_str(), "PhysicalDevice", physDev ) ) break;
            width = config.Get( section.c_str(), "Width", 1650u );
            height = config.Get( section.c_str(), "Height", 1050u );
            OpenWindow( physDev, width, height, section.c_str() );
            windowAdded = true;
            idx++;
        }
        CheckPanic( windowAdded, "No windows to open configured in backend-wayland.ini." );
    }
    else
    {
        OpenWindow( -1, 1650, 1050, "ModernCore" );
    }

    m_dpy->Roundtrip();
}

void BackendWayland::Run()
{
    for( auto& window : m_windows ) window->InvokeRender();
    m_dpy->Run();
}

void BackendWayland::Stop()
{
    m_dpy->Stop();
}

void BackendWayland::PointerMotion( double x, double y )
{
    //m_window->PointerMotion( x * m_scale, y * m_scale );
}

void BackendWayland::OpenWindow( int physDev, uint32_t width, uint32_t height, const char* title )
{
    auto& server = Server::Instance();
    auto& vkInstance = server.VkInstance();

    auto window = std::make_unique<WaylandWindow>( *m_dpy, vkInstance );

    std::shared_ptr<GpuDevice> gpu;
    if( physDev < 0 )
    {
        const auto& physicalDevices = vkInstance.QueryPhysicalDevices();

        auto device_ = PhysDevSel::PickBest( physicalDevices, window->VkSurface() );
        CheckPanic( device_, "Failed to find suitable physical device" );
        auto device = (VkPhysicalDevice)*device_;

        auto it = std::find_if( m_gpus.begin(), m_gpus.end(), [device]( const auto& v ) { return *v->Device() == device; } );
        CheckPanic( it != m_gpus.end(), "Selected physical device has valid index, but not found in list of GPUs (?)" );

        physDev = it - m_gpus.begin();
        mclog( LogLevel::Info, "Auto-selected best physical device: %i", physDev );
        gpu = *it;
    }
    else
    {
        CheckPanic( physDev < m_gpus.size(), "Invalid physical device index set in backend-wayland.ini. Value: %i, max: %zu.", physDev, m_gpus.size() - 1 );
        gpu = m_gpus[physDev];
        CheckPanic( gpu->IsPresentSupported( window->VkSurface() ), "Selected physical device does not support presentation to Wayland surface" );
    }

    mclog( LogLevel::Info, "Opening window on physical device %i, size %" PRIu32 "x%" PRIu32, physDev, width, height );

    static constexpr WaylandWindow::Listener listener = {
        .OnClose = Method( Close ),
        .OnRender = Method( Render )
    };

    window->SetListener( &listener, this );
    window->SetDevice( gpu->Device(), VkExtent2D{ width, height } );
    window->SetTitle( title );
    window->SetAppId( "ModernCore" );
    window->LockSize();
    window->Commit();
    m_windows.emplace_back( std::move( window ) );
}

void BackendWayland::Close( WaylandWindow* )
{
    // TODO
    m_dpy->Stop();
}

bool BackendWayland::Render( WaylandWindow* window )
{
#ifdef TRACY_ENABLE
    ZoneScoped;
    char buf[128];
    const auto len = snprintf( buf, 128, "Render %s", window->GetTitle() );
    ZoneName( buf, len );
#endif

    auto& cmdbuf = window->BeginFrame();

    const auto& extent = window->GetExtent();

    VkRenderingAttachmentInfo attachmentInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = window->GetImageView(),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {{{ 0.25f, 0.25f, 0.25f, 1.0f }}}
    };

    VkRenderingInfo renderingInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = { { 0, 0 }, extent },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachmentInfo
    };

    vkCmdBeginRendering( cmdbuf, &renderingInfo );

#ifdef TRACY_ENABLE
    auto tracyCtx = window->Device().GetTracyContext();
    tracy::VkCtxScope* tracyScope = nullptr;
    if( tracyCtx ) ZoneVkNew( tracyCtx, tracyScope, cmdbuf, "Render", true );
#endif

    //for( auto& renderable : Server::Instance().Renderables() ) renderable->Render( *this, cmdbuf );

    vkCmdEndRendering( cmdbuf );

#ifdef TRACY_ENABLE
    delete tracyScope;
#endif

    window->EndFrame();

    return true;
}
