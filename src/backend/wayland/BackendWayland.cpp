#include <format>
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
            if( !config.GetOpt( section.c_str(), "PhysicalDevice", physDev ) ) break;
            OpenWindow( physDev );
            windowAdded = true;
            idx++;
        }
        CheckPanic( windowAdded, "No windows to open configured in backend-wayland.ini." );
    }
    else
    {
        OpenWindow();
    }
}

void BackendWayland::Run( const std::function<void()>& render )
{
    for( auto& window : m_windows ) window->Show( render );
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

void BackendWayland::OpenWindow( int physDev )
{
    mclog( LogLevel::Info, "Opening window on physical device %i", physDev );

    static constexpr WaylandWindow::Listener listener = {
        .OnClose = Method( Close )
    };

    auto window = std::make_unique<WaylandBackendWindow>( *this, Server::Instance().VkInstance(), WaylandBackendWindow::Params { physDev, *this } );
    window->SetListener( &listener, this );
    m_windows.emplace_back( std::move( window ) );
}

void BackendWayland::Close( WaylandWindow* )
{
    m_keepRunning = false;
}
