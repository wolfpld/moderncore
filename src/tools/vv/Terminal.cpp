#include <array>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include "Terminal.hpp"
#include "util/Panic.hpp"

constexpr std::array termFileNo = { STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO };

// s_termFd: terminal device re-opened O_RDWR.
// s_readFd/s_writeFd: the fds queries are actually read/written on.
static int s_termFd = -1;
static int s_readFd = -1;
static int s_writeFd = -1;
static struct termios s_termSave;

static void ResetFds()
{
    if( s_termFd >= 0 ) close( s_termFd );
    s_termFd = s_readFd = s_writeFd = -1;
}

static bool PickInheritedFds()
{
    int writeFd = -1;
    for( auto fd : { STDOUT_FILENO, STDERR_FILENO } )
    {
        if( isatty( fd ) )
        {
            writeFd = fd;
            break;
        }
    }
    if( writeFd < 0 ) return false;

    struct stat wst;
    if( fstat( writeFd, &wst ) != 0 ) return false;

    for( auto fd : termFileNo )
    {
        if( !isatty( fd ) ) continue;

        struct stat rst;
        if( fstat( fd, &rst ) != 0 || rst.st_rdev != wst.st_rdev ) continue;

        const int acc = fcntl( fd, F_GETFL );
        if( acc != -1 && ( acc & O_ACCMODE ) != O_WRONLY )
        {
            s_readFd = fd;
            s_writeFd = writeFd;
            return true;
        }
    }
    return false;
}

bool OpenTerminal()
{
    CheckPanic( s_readFd < 0, "Terminal already open" );

    for( auto termfd : termFileNo )
    {
        if( isatty( termfd ) )
        {
            auto name = ttyname( termfd );
            if( name )
            {
                int fd = open( name, O_RDWR );
                if( fd != -1 )
                {
                    mclog( LogLevel::Info, "Opened terminal: %s", name );
                    s_termFd = fd;
                    break;
                }
            }
        }
    }

    if( s_termFd >= 0 )
    {
        s_readFd = s_writeFd = s_termFd;
    }
    else if( PickInheritedFds() )
    {
        mclog( LogLevel::Info, "Terminal not re-openable, using inherited fds: read %d, write %d", s_readFd, s_writeFd );
    }
    else
    {
        return false;
    }

    if( tcgetattr( s_writeFd, &s_termSave ) != 0 )
    {
        ResetFds();
        return false;
    }

    struct termios tio = s_termSave;
    tio.c_lflag &= ~( ICANON | ECHO );
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;

    if( tcsetattr( s_writeFd, TCSANOW, &tio ) != 0 )
    {
        ResetFds();
        return false;
    }

    return true;
}

void CloseTerminal()
{
    CheckPanic( s_readFd >= 0, "Terminal not open" );
    tcsetattr( s_writeFd, TCSAFLUSH, &s_termSave );
    ResetFds();
}

std::string QueryTerminal( const char* query )
{
    CheckPanic( s_writeFd >= 0, "Terminal not open" );

    const auto sz = strlen( query );
    if( write( s_writeFd, query, sz ) != sz ) return {};

    return QueryTerminal();
}

std::string QueryTerminal()
{
    std::string ret;
    char buf[1024];
    while( true )
    {
        struct pollfd pfd = { .fd = s_readFd, .events = POLLIN };
        const auto pr = poll( &pfd, 1, 1000 );
        if( pr < 0 ) return {};
        if( pr == 0 ) break;

        const auto rd = read( s_readFd, buf, sizeof( buf ) );
        if( rd < 0 ) return {};
        ret.append( buf, rd );
        if( rd < sizeof( buf ) ) break;
    }

    return ret;
}
