#include <errno.h>
#include <sys/stat.h>

#include "Filesystem.hpp"

bool CreateDirectories( const std::string& path )
{
    struct stat buf;
    if( stat( path.c_str(), &buf ) == 0 ) return true;
    if( errno != ENOENT ) return false;

    size_t pos = 0;
    do
    {
        pos = path.find( '/', pos+1 );
        if( mkdir( path.substr( 0, pos ).c_str(), S_IRWXU ) != 0 )
        {
            if( errno != EEXIST ) return false;
        }
    }
    while( pos != std::string::npos );

    return true;
}
