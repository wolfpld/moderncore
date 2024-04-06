#include <algorithm>
#include <format>
#include <tracy/Tracy.hpp>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_wayland.h>

#include "BackendWayland.hpp"
#include "WaylandConnector.hpp"
#include "WaylandOutput.hpp"
#include "WaylandBackendWindow.hpp"
#include "render/SoftwareCursor.hpp"
#include "server/GpuDevice.hpp"
#include "server/Server.hpp"
#include "util/Invoke.hpp"
#include "util/Panic.hpp"
#include "util/Tracy.hpp"
#include "vulkan/ext/PhysDevSel.hpp"
#include "vulkan/VlkError.hpp"
#include "vulkan/VlkInstance.hpp"

WaylandBackendWindow::WaylandBackendWindow( WaylandDisplay& display, VlkInstance& vkInstance, Params&& p )
    : WaylandWindow( display, vkInstance )
    , m_backend( p.backend )
{
    ZoneScoped;

    if( !display.FractionalScaleManager() || !display.Viewporter() )
    {
        static constexpr wl_surface_listener surfaceListener = {
            .enter = Method( Enter ),
            .leave = Method( Leave )
        };

        wl_surface_add_listener( Surface(), &surfaceListener, this );
    }

    static int windowCount = 0;

    m_id = ++windowCount;
    const auto title = std::format( "ModernCore #{}", m_id );
    ZoneText( title.c_str(), title.size() );

    SetAppId( "moderncore" );
    SetTitle( title.c_str() );
    Commit();

    wl_display_roundtrip( display.Display() );

    auto& server = Server::Instance();
    auto& gpuList = server.Gpus();
    if( p.physDev < 0 )
    {
        const auto& physicalDevices = vkInstance.QueryPhysicalDevices();

        auto device_ = PhysDevSel::PickBest( physicalDevices, VkSurface() );
        CheckPanic( device_, "Failed to find suitable physical device" );
        auto device = (VkPhysicalDevice)*device_;

        auto it = std::find_if( gpuList.begin(), gpuList.end(), [device]( const auto& v ) { return v->Device() == device; } );
        CheckPanic( it != gpuList.end(), "Selected physical device has valid index, but not found in list of GPUs (?)" );

        mclog( LogLevel::Info, "Selected physical device: %i", it - gpuList.begin() );
        m_gpu = *it;
    }
    else
    {
        CheckPanic( p.physDev < gpuList.size(), "Invalid physical device index set in backend-wayland.ini. Value: %i, max: %zu.", p.physDev, gpuList.size() - 1 );
        m_gpu = gpuList[p.physDev];
        CheckPanic( m_gpu->IsPresentSupported( VkSurface() ), "Selected physical device does not support presentation to Wayland surface" );
    }

    ZoneVkDevice( m_gpu->Device().GetPhysicalDevice() );

    m_connector = std::make_shared<WaylandConnector>( m_gpu->Device(), VkSurface(), p.width, p.height );
    m_gpu->AddConnector( m_connector );
}

WaylandBackendWindow::~WaylandBackendWindow()
{
    m_gpu->RemoveConnector( m_connector );
    m_connector.reset();
}

void WaylandBackendWindow::RenderCursor( VkCommandBuffer cmdBuf, CursorLogic& cursorLogic )
{
    m_cursor->Render( cmdBuf, cursorLogic );
}

void WaylandBackendWindow::PointerMotion( double x, double y )
{
    m_cursor->SetPosition( x, y );
}

void WaylandBackendWindow::Enter( struct wl_surface* surface, struct wl_output* output )
{
    CheckPanic( !FractionalScale(), "Outputs should not be tracked when fractional scaling is enabled" );

    auto& outputMap = m_backend.OutputMap();
    auto it = std::find_if( outputMap.begin(), outputMap.end(), [output]( const auto& pair ) { return pair.second->GetOutput() == output; } );
    CheckPanic( it != outputMap.end(), "Output not found" );
    m_outputs.insert( it->first );

    const auto outputScale = it->second->GetScale();
    if( outputScale > m_scale )
    {
        mclog( LogLevel::Info, "Window %i: Enter output %i, setting scale to %i", m_id, it->first, outputScale );
        m_scale = outputScale;
        wl_surface_set_buffer_scale( Surface(), m_scale );
        wl_surface_commit( Surface() );
    }
}

void WaylandBackendWindow::Leave( struct wl_surface* surface, struct wl_output* output )
{
    CheckPanic( !FractionalScale(), "Outputs should not be tracked when fractional scaling is enabled" );

    auto& outputMap = m_backend.OutputMap();
    auto it = std::find_if( outputMap.begin(), outputMap.end(), [output]( const auto& pair ) { return pair.second->GetOutput() == output; } );
    CheckPanic( it != outputMap.end(), "Output not found" );
    m_outputs.erase( it->first );

    if( it->second->GetScale() == m_scale )
    {
        int scale = 1;
        for( const auto& outputId : m_outputs )
        {
            const auto oit = outputMap.find( outputId );
            CheckPanic( oit != outputMap.end(), "Output not found" );
            const auto outputScale = oit->second->GetScale();
            if( outputScale > scale ) scale = outputScale;
        }
        if( scale != m_scale )
        {
            mclog( LogLevel::Info, "Window %i: Leave output %i, setting scale to %i", m_id, it->first, scale );
            m_scale = scale;
            wl_surface_set_buffer_scale( Surface(), m_scale );
            wl_surface_commit( Surface() );
        }
    }
}
