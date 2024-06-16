#include <format>
#include <inttypes.h>
#include <tracy/Tracy.hpp>

#include "BackendWayland.hpp"
#include "server/GpuDevice.hpp"
#include "server/Renderable.hpp"
#include "server/Server.hpp"
#include "util/Config.hpp"
#include "util/Invoke.hpp"
#include "util/Panic.hpp"
#include "util/Tracy.hpp"
#include "vulkan/VlkCommandBuffer.hpp"
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
}

void BackendWayland::Run()
{
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

    auto& gpuList = server.Gpus();
    std::shared_ptr<GpuDevice> gpu;

    if( physDev < 0 )
    {
        const auto& physicalDevices = vkInstance.QueryPhysicalDevices();

        auto device_ = PhysDevSel::PickBest( physicalDevices, window->VkSurface() );
        CheckPanic( device_, "Failed to find suitable physical device" );
        auto device = (VkPhysicalDevice)*device_;

        auto it = std::find_if( gpuList.begin(), gpuList.end(), [device]( const auto& v ) { return *v->Device() == device; } );
        CheckPanic( it != gpuList.end(), "Selected physical device has valid index, but not found in list of GPUs (?)" );

        mclog( LogLevel::Info, "Selected physical device: %i", it - gpuList.begin() );
        gpu = *it;
    }
    else
    {
        CheckPanic( physDev < gpuList.size(), "Invalid physical device index set in backend-wayland.ini. Value: %i, max: %zu.", physDev, gpuList.size() - 1 );
        gpu = gpuList[physDev];
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
    window->Commit();
    m_windows.emplace_back( std::move( window ) );
}

void BackendWayland::Close( WaylandWindow* )
{
    // TODO
    m_dpy->Stop();
}

void BackendWayland::Render( WaylandWindow* window )
{
    auto& cmdbuf = window->BeginFrame();

    VkRenderingAttachmentInfo attachmentInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = window->GetImageView(),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {{{ 0.25f, 0.25f, 0.25f, 1.0f }}}
    };

    // TODO
    VkRenderingInfo renderingInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = { { 0, 0 }, { 1680, 1050 } },
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
}
