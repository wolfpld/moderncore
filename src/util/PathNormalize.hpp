#pragma once

#include <string>

inline std::string NormalizePath( const char* path )
{
    if( path[0] != '/' ) return {};

    const char* end = path;
    while( *end ) end++;

    std::string res;
    const char* ptr = path;

    while( ptr < end )
    {
        const char* next = ptr;
        while( next < end && *next != '/' ) next++;
        const auto lsz = next - ptr;

        if( lsz == 2 && ptr[0] == '.' && ptr[1] == '.' )
        {
            if( !res.empty() ) res.resize( res.find_last_of( '/' ) );
            ptr = next + 1;
            continue;
        }

        if( lsz == 1 && *ptr == '.' )
        {
            ptr = next + 1;
            continue;
        }

        if( lsz != 0 )
        {
            res += '/';
            res.append( ptr, lsz );
        }
        ptr = next + 1;
    }

    if( res.empty() ) return "/";
    return res;
}
