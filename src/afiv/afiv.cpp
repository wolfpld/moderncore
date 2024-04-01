#include <getopt.h>
#include <memory>
#include <thread>

#include "util/ArgParser.hpp"
#include "util/Logs.hpp"
#include "util/Panic.hpp"
#include "util/TaskDispatch.hpp"
#include "vulkan/VlkInstance.hpp"
#include "wayland/WaylandDisplay.hpp"


std::unique_ptr<TaskDispatch> g_dispatch;
std::unique_ptr<VlkInstance> g_vkInstance;
std::unique_ptr<WaylandDisplay> g_waylandDisplay;


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
        { "validation", required_argument, nullptr, 'V' },
    };

    int opt;
    while( ( opt = getopt_long( argc, argv, "dV:", longOptions, nullptr ) ) != -1 )
    {
        switch (opt)
        {
        case 'd':
            SetLogLevel( LogLevel::Callstack );
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

    g_vkInstance->InitPhysicalDevices( *g_dispatch );
    const auto& devices = g_vkInstance->QueryPhysicalDevices();
    CheckPanic( !devices.empty(), "No physical devices found" );

    mclog( LogLevel::Info, "Found %d physical devices", devices.size() );


    return 0;
}
