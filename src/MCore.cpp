#include <getopt.h>

#include "server/Server.hpp"
#include "util/Ansi.hpp"
#include "util/Callstack.hpp"
#include "util/Logs.hpp"

namespace {
void PrintHelp()
{
    printf( "Usage: mcore [options]\n" );
    printf( "Options:\n" );
    printf( "  -d, --debug              Enable debug logging\n" );
    printf( "  -e, --external           Show external callstacks\n" );
    printf( "  -s, --single-thread      Run server in single-thread mode\n" );
    printf( "  --help                   Print this help\n" );
}
}

int main( int argc, char** argv )
{
#ifdef NDEBUG
    SetLogLevel( LogLevel::Error );
#endif

    enum { OptHelp };

    struct option longOptions[] = {
        { "debug", no_argument, nullptr, 'd' },
        { "external", no_argument, nullptr, 'e' },
        { "single-thread", no_argument, nullptr, 's' },
        { "help", no_argument, nullptr, OptHelp },
        {}
    };

    bool singleThread = false;

    int opt;
    while( ( opt = getopt_long( argc, argv, "des", longOptions, nullptr ) ) != -1 )
    {
        switch (opt)
        {
        case 'd':
            SetLogLevel( LogLevel::Callstack );
            break;
        case 'e':
            ShowExternalCallstacks( true );
            break;
        case 's':
            singleThread = true;
            break;
        case OptHelp:
            PrintHelp();
            return 0;
        default:
            break;
        }
    }

    printf( "Starting " ANSI_BOLD ANSI_ITALIC "Modern Core" ANSI_RESET "...\n\n" );

    Server server( singleThread );
    server.Run();
    return 0;
}
