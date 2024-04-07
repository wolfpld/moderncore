#include <getopt.h>
#include <memory>
#include <thread>

#include "util/ArgParser.hpp"
#include "util/Callstack.hpp"
#include "util/Logs.hpp"
#include "util/Panic.hpp"
#include "util/TaskDispatch.hpp"
#include "vulkan/VlkDevice.hpp"
#include "vulkan/VlkInstance.hpp"
#include "vulkan/ext/PhysDevSel.hpp"
#include "wayland/WaylandDisplay.hpp"
#include "wayland/WaylandWindow.hpp"


std::unique_ptr<TaskDispatch> g_dispatch;
std::unique_ptr<VlkInstance> g_vkInstance;
std::unique_ptr<WaylandDisplay> g_waylandDisplay;
std::unique_ptr<WaylandWindow> g_waylandWindow;
std::unique_ptr<VlkDevice> g_vkDevice;

bool g_keepRunning = true;


int main( int argc, char** argv )
{
#ifdef NDEBUG
    SetLogLevel( LogLevel::Error );
#endif

#ifdef DEBUG
    bool enableValidation = true;
#else
    bool enableValidation = false;
#endif

    struct option longOptions[] = {
        { "debug", no_argument, nullptr, 'd' },
        { "external", no_argument, nullptr, 'e' },
        { "validation", required_argument, nullptr, 'V' },
    };

    int opt;
    while( ( opt = getopt_long( argc, argv, "deV:", longOptions, nullptr ) ) != -1 )
    {
        switch (opt)
        {
        case 'd':
            SetLogLevel( LogLevel::Callstack );
            break;
        case 'e':
            ShowExternalCallstacks( true );
            break;
        case 'V':
            enableValidation = ParseBoolean( optarg );
            break;
        default:
            return 1;
        }
    }

    auto vulkanThread = std::thread( [enableValidation] {
        g_vkInstance = std::make_unique<VlkInstance>( VlkInstanceType::Wayland, enableValidation );
    } );
    auto dispatchThread = std::thread( [] {
        const auto cpus = std::thread::hardware_concurrency();
        g_dispatch = std::make_unique<TaskDispatch>( cpus == 0 ? 0 : cpus - 1, "Worker" );
    } );

    g_waylandDisplay = std::make_unique<WaylandDisplay>();
    g_waylandDisplay->Connect();

    dispatchThread.join();
    vulkanThread.join();

    static constexpr WaylandWindow::Listener listener = {
        .OnClose = [] (void*, WaylandWindow*) { g_keepRunning = false; },
    };

    g_waylandWindow = std::make_unique<WaylandWindow>( *g_waylandDisplay, *g_vkInstance );
    g_waylandWindow->SetListener( &listener, nullptr );
    g_waylandWindow->SetTitle( "AFIV" );
    g_waylandWindow->SetAppId( "afiv" );

    g_vkInstance->InitPhysicalDevices( *g_dispatch );
    const auto& devices = g_vkInstance->QueryPhysicalDevices();
    CheckPanic( !devices.empty(), "No physical devices found" );
    mclog( LogLevel::Info, "Found %d physical devices", devices.size() );

    auto best = PhysDevSel::PickBest( g_vkInstance->QueryPhysicalDevices(), g_waylandWindow->VkSurface(), PhysDevSel::RequireGraphic );
    CheckPanic( best, "Failed to find suitable physical device" );

    g_vkDevice = std::make_unique<VlkDevice>( *g_vkInstance, best, VlkDevice::RequireGraphic | VlkDevice::RequirePresent, g_waylandWindow->VkSurface() );

    return 0;
}
