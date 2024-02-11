#include <getopt.h>

#include "server/Server.hpp"
#include "util/Ansi.hpp"
#include "util/Callstack.hpp"
#include "util/Logs.hpp"

int main( int argc, char** argv )
{
    printf( "Starting " ANSI_BOLD ANSI_ITALIC "Modern Core" ANSI_RESET "...\n\n" );

    struct option longOptions[] = {
        { "debug", no_argument, nullptr, 'd' },
        { "external", no_argument, nullptr, 'e' },
        { "single-thread", no_argument, nullptr, 's' },
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
        default:
            break;
        }
    }

    Server server( singleThread );
    server.Run();
    return 0;
}
