#include <pwd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "Alloca.h"
#include "Home.hpp"

std::string GetHome()
{
    auto home = getenv( "HOME" );
    if( home ) return home;

    auto bufSz = sysconf( _SC_GETPW_R_SIZE_MAX );
    if( bufSz == -1 ) bufSz = 16 * 1024;
    auto buf = (char*)alloca( bufSz );

    struct passwd pass;
    struct passwd* res;
    if( getpwuid_r( geteuid(), &pass, buf, bufSz, &res ) != 0 ) res = nullptr;

    return res ? pass.pw_dir : "";
}

std::string ExpandHome( const char* path )
{
    if( path[0] != '~' ) return path;
    return GetHome() + (path+1);
}