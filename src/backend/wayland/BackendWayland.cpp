#include <format>
#include <inttypes.h>
#include <string.h>
#include <tracy/Tracy.hpp>

#include "BackendWayland.hpp"
#include "WaylandOutput.hpp"
#include "WaylandBackendWindow.hpp"
#include "server/Server.hpp"
#include "util/Config.hpp"
#include "util/Invoke.hpp"
#include "util/Panic.hpp"
#include "wayland/WaylandRegistry.hpp"

BackendWayland::BackendWayland()
{
    Connect();
}

BackendWayland::~BackendWayland()
{
    m_windows.clear();
    m_outputMap.clear();
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
            OpenWindow( physDev, width, height );
            windowAdded = true;
            idx++;
        }
        CheckPanic( windowAdded, "No windows to open configured in backend-wayland.ini." );
    }
    else
    {
        OpenWindow( -1, 1650, 1050 );
    }
}

void BackendWayland::Run( const std::function<void()>& render )
{
    m_render = render;
    m_keepRunning = true;
    for( auto& window : m_windows )
    {
        m_render();
        window->Commit();
    }
    while( m_keepRunning && wl_display_dispatch( Display() ) != -1 ) {}
}

void BackendWayland::Stop()
{
    m_keepRunning = false;
}

void BackendWayland::RegistryGlobal( wl_registry* reg, uint32_t name, const char* interface, uint32_t version )
{
    if( strcmp( interface, wl_output_interface.name ) == 0 )
    {
        auto output = RegistryBind( wl_output, 3, 4 );
        m_outputMap.emplace( name, std::make_unique<WaylandOutput>( output, name ) );
    }
    else
    {
        WaylandDisplay::RegistryGlobal( reg, name, interface, version );
    }
}

void BackendWayland::RegistryGlobalRemove( wl_registry* reg, uint32_t name )
{
    auto it = m_outputMap.find( name );
    if( it != m_outputMap.end() ) m_outputMap.erase( it );
}

void BackendWayland::PointerMotion( double x, double y )
{
    //m_window->PointerMotion( x * m_scale, y * m_scale );
}

void BackendWayland::OpenWindow( int physDev, uint32_t width, uint32_t height )
{
    mclog( LogLevel::Info, "Opening window on physical device %i, size %" PRIu32 "x%" PRIu32, physDev, width, height );

    static constexpr WaylandWindow::Listener listener = {
        .OnClose = Method( Close ),
        .OnRender = Method( Render )
    };

    auto window = std::make_unique<WaylandBackendWindow>( *this, Server::Instance().VkInstance(), WaylandBackendWindow::Params { physDev, width, height, *this } );
    window->SetListener( &listener, this );
    m_windows.emplace_back( std::move( window ) );
}

void BackendWayland::Close( WaylandWindow* )
{
    m_keepRunning = false;
}

void BackendWayland::Render( WaylandWindow* )
{
    m_render();
}
