#include "Viewport.hpp"
#include "util/Invoke.hpp"
#include "util/Panic.hpp"
#include "vulkan/VlkDevice.hpp"
#include "vulkan/VlkInstance.hpp"
#include "vulkan/ext/PhysDevSel.hpp"
#include "wayland/WaylandDisplay.hpp"

Viewport::Viewport( WaylandDisplay& display, VlkInstance& vkInstance )
    : m_display( display )
    , m_vkInstance( vkInstance )
    , m_window( std::make_unique<WaylandWindow>( display, vkInstance ) )
{
    static constexpr WaylandWindow::Listener listener = {
        .OnClose = Method( Close ),
        .OnRender = Method( Render ),
        .OnScale = Method( Scale ),
        .OnResize = Method( Resize )
    };

    m_window->SetListener( &listener, this );
    m_window->SetAppId( "iv" );
    m_window->SetTitle( "IV" );
    m_window->Commit();
    m_display.Roundtrip();

    const auto& devices = m_vkInstance.QueryPhysicalDevices();
    CheckPanic( !devices.empty(), "No Vulkan physical devices found" );
    mclog( LogLevel::Info, "Found %d Vulkan physical devices", devices.size() );

    auto best = PhysDevSel::PickBest( devices, m_window->VkSurface(), PhysDevSel::RequireGraphic );
    CheckPanic( best, "Failed to find suitable Vulkan physical device" );

    m_device = std::make_shared<VlkDevice>( m_vkInstance, best, VlkDevice::RequireGraphic | VlkDevice::RequirePresent, m_window->VkSurface() );
    m_window->SetDevice( m_device, VkExtent2D { 256, 256 } );
    m_window->InvokeRender();
}

Viewport::~Viewport()
{
}

void Viewport::Close( WaylandWindow* window )
{
    CheckPanic( window == m_window.get(), "Invalid window" );
    m_display.Stop();
}

bool Viewport::Render( WaylandWindow* window )
{
    CheckPanic( window == m_window.get(), "Invalid window" );
    auto& cmdbuf = window->BeginFrame( true );
    window->EndFrame();
    return true;
}

void Viewport::Scale( WaylandWindow* window, uint32_t scale )
{
    CheckPanic( window == m_window.get(), "Invalid window" );
}

void Viewport::Resize( WaylandWindow* window, uint32_t width, uint32_t height )
{
    CheckPanic( window == m_window.get(), "Invalid window" );
    window->Resize( width, height );
}
