#include "server/Server.hpp"
#include "util/Ansi.hpp"

int main()
{
    printf( "Starting " ANSI_BOLD ANSI_ITALIC "Modern Core" ANSI_RESET "...\n\n" );

    Server server;
    server.Run();
    return 0;
}
