#include <getopt.h>

#include "GitRef.hpp"
#include "server/Server.hpp"
#include "util/Ansi.hpp"
#include "util/ArgParser.hpp"
#include "util/Callstack.hpp"
#include "util/Logs.hpp"

static void PrintHelp()
{
    printf( "Usage: mcore [options]\n" );
    printf( "Options:\n" );
    printf( "  -d, --debug                  Enable debug logging\n" );
    printf( "  -e, --external               Show external callstacks\n" );
    printf( "  -s, --sync-logs              Synchronize log output\n" );
    printf( "  -l, --log-file               Log to file\n" );
    printf( "  -V, --validation [on|off]    Enable or disable validation layers\n" );
    printf( "  --help                       Print this help\n" );
}

int main( int argc, char** argv )
{
#ifdef NDEBUG
    SetLogLevel( LogLevel::Error );
#endif

#ifdef NDEBUG
    bool enableValidation = false;
#else
    bool enableValidation = true;
#endif

    enum { OptHelp };

    struct option longOptions[] = {
        { "debug", no_argument, nullptr, 'd' },
        { "external", no_argument, nullptr, 'e' },
        { "sync-logs", no_argument, nullptr, 's' },
        { "log-file", no_argument, nullptr, 'l' },
        { "validation", required_argument, nullptr, 'V' },
        { "help", no_argument, nullptr, OptHelp },
        {}
    };

    int opt;
    while( ( opt = getopt_long( argc, argv, "deslV:", longOptions, nullptr ) ) != -1 )
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
            SetLogSynchronized( true );
            break;
        case 'l':
            SetLogToFile( true );
            break;
        case 'V':
            enableValidation = ParseBoolean( optarg );
            break;
        case OptHelp:
            PrintHelp();
            return 0;
        default:
            return 1;
        }
    }

    printf( "Starting " ANSI_BOLD ANSI_ITALIC "Modern Core" ANSI_RESET "…\n\n" );
    printf( "Build id: %s\n\n", GitRef );

    Server server( enableValidation );
    server.Run();
    return 0;
}
