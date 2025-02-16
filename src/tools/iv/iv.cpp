#include <getopt.h>
#include <memory>

#include "Viewport.hpp"
#include "util/ArgParser.hpp"
#include "util/Callstack.hpp"
#include "util/Logs.hpp"
#include "vulkan/VlkInstance.hpp"
#include "wayland/WaylandDisplay.hpp"

static void PrintHelp()
{
    printf( "Usage: iv [options]\n" );
    printf( "Options:\n" );
    printf( "  -d, --debug                  Enable debug output\n" );
    printf( "  -e, --external               Show external callstacks\n" );
    printf( "  -V, --validation [on|off]    Enable or disable Vulkan validation layers\n" );
}

int main( int argc, char** argv )
{
#ifdef NDEBUG
    SetLogLevel( LogLevel::Error );
    bool enableValidation = false;
#else
    bool enableValidation = true;
#endif

    struct option longOptions[] = {
        { "debug", no_argument, nullptr, 'd' },
        { "external", no_argument, nullptr, 'e' },
        { "validation", required_argument, nullptr, 'V' },
        {}
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
            PrintHelp();
            return 1;
        }
    }

    auto vkInstance = std::make_unique<VlkInstance>( VlkInstanceType::Wayland, enableValidation );

    auto waylandDisplay = std::make_unique<WaylandDisplay>();
    waylandDisplay->Connect();

    auto viewport = std::make_unique<Viewport>( *waylandDisplay, *vkInstance );
    waylandDisplay->Run();

    return 0;
}
