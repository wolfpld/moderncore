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
        {}
    };

    int opt;
    while( ( opt = getopt_long( argc, argv, "de", longOptions, nullptr ) ) != -1 )
    {
        switch (opt)
        {
        case 'd':
            SetLogLevel( LogLevel::Debug );
            break;
        case 'e':
            ShowExternalCallstacks( true );
            break;
        default:
            break;
        }
    }

    Server server;
    server.Run();
    return 0;
}
