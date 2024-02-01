#include <getopt.h>

#include "server/Server.hpp"
#include "util/Ansi.hpp"
#include "util/Logs.hpp"

int main( int argc, char** argv )
{
    printf( "Starting " ANSI_BOLD ANSI_ITALIC "Modern Core" ANSI_RESET "...\n\n" );

    struct option longOptions[] = {
        { "debug", no_argument, nullptr, 'd' },
        {}
    };

    int opt;
    while( ( opt = getopt_long( argc, argv, "d", longOptions, nullptr ) ) != -1 )
    {
        switch (opt)
        {
        case 'd':
            SetLogLevel( LogLevel::Debug );
            break;
        default:
            break;
        }
    }

    Server server;
    server.Run();
    return 0;
}
