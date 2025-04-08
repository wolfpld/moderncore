#include <getopt.h>
#include <memory>
#include <stdlib.h>
#include <vector>

#include "GitRef.hpp"
#include "Viewport.hpp"
#include "util/Ansi.hpp"
#include "util/ArgParser.hpp"
#include "util/Callstack.hpp"
#include "util/Home.hpp"
#include "util/Logs.hpp"
#include "vulkan/VlkInstance.hpp"
#include "wayland/WaylandDisplay.hpp"

static void PrintHelp()
{
    printf( ANSI_BOLD "iv — Wayland HDR image viewer, build %s" ANSI_RESET "\n\n", GitRef );
    printf( "Usage: iv [options]\n" );
    printf( "Options:\n" );
    printf( "  -d, --debug                  Enable debug output\n" );
    printf( "  -e, --external               Show external callstacks\n" );
    printf( "  -V, --validation [on|off]    Enable or disable Vulkan validation layers\n" );
    printf( "  -g, --gpu [id]               Select GPU by id\n" );
    printf( "  --help                       Print this help\n" );
}

int main( int argc, char** argv )
{
#ifdef NDEBUG
    SetLogLevel( LogLevel::Warning );
    bool enableValidation = false;
#else
    bool enableValidation = true;
#endif

    int gpu = -1;

    enum { OptHelp };

    struct option longOptions[] = {
        { "debug", no_argument, nullptr, 'd' },
        { "external", no_argument, nullptr, 'e' },
        { "validation", required_argument, nullptr, 'V' },
        { "gpu", required_argument, nullptr, 'g' },
        { "help", no_argument, nullptr, OptHelp },
        {}
    };

    int opt;
    while( ( opt = getopt_long( argc, argv, "deV:g:", longOptions, nullptr ) ) != -1 )
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
        case 'g':
            gpu = atoi( optarg );
            break;
        case OptHelp:
            PrintHelp();
            return 0;
        default:
            PrintHelp();
            return 1;
        }
    }

    setenv( "ENABLE_HDR_WSI", "1", 0 );

    auto vkInstance = std::make_unique<VlkInstance>( VlkInstanceType::Wayland, enableValidation );
    auto waylandDisplay = std::make_unique<WaylandDisplay>();

    auto viewport = std::make_unique<Viewport>( *waylandDisplay, *vkInstance, gpu );
    if( optind != argc )
    {
        const auto sz = argc - optind;
        if( sz == 1 )
        {
            const auto path = argv[optind];
            if( path[0] == '~' )
            {
                viewport->LoadImage( ExpandHome( path ).c_str(), true );
            }
            else
            {
                viewport->LoadImage( argv[optind], true );
            }
        }
        else
        {
            std::vector<std::string> files;
            for( int i = optind; i < argc; ++i ) files.emplace_back( ExpandHome( argv[i] ) );
            viewport->LoadImage( files );
        }
    }

    waylandDisplay->Run();

    return 0;
}
