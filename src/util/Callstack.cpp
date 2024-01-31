#include <cxxabi.h>
#include <stdint.h>
#include <string.h>

#include "Callstack.hpp"
#include "Logs.hpp"
#include "contrib/libbacktrace/backtrace.h"

static int callstackIdx;

constexpr const char* TypesList[] = {
    "bool ", "char ", "double ", "float ", "int ", "long ", "short ",
    "signed ", "unsigned ", "void ", "wchar_t ", "size_t ", "int8_t ",
    "int16_t ", "int32_t ", "int64_t ", "intptr_t ", "uint8_t ", "uint16_t ",
    "uint32_t ", "uint64_t ", "ptrdiff_t ", nullptr
};

static char* NormalizePath( const char* path )
{
    if( path[0] != '/' ) return nullptr;

    const char* ptr = path;
    const char* end = path;
    while( *end ) end++;

    char* res = (char*)malloc( end - ptr + 1 );
    size_t rsz = 0;

    while( ptr < end )
    {
        const char* next = ptr;
        while( next < end && *next != '/' ) next++;
        size_t lsz = next - ptr;
        switch( lsz )
        {
        case 2:
            if( memcmp( ptr, "..", 2 ) == 0 )
            {
                const char* back = res + rsz - 1;
                while( back > res && *back != '/' ) back--;
                rsz = back - res;
                ptr = next + 1;
                continue;
            }
            break;
        case 1:
            if( *ptr == '.' )
            {
                ptr = next + 1;
                continue;
            }
            break;
        case 0:
            ptr = next + 1;
            continue;
        }
        if( rsz != 1 ) res[rsz++] = '/';
        memcpy( res+rsz, ptr, lsz );
        rsz += lsz;
        ptr = next + 1;
    }

    if( rsz == 0 )
    {
        memcpy( res, "/", 2 );
    }
    else
    {
        res[rsz] = '\0';
    }
    return res;
}

static int CallstackCallback( void*, uintptr_t pc, const char* filename, int lineno, const char* function )
{
    if( function )
    {
        auto demangled = abi::__cxa_demangle( function, nullptr, nullptr, nullptr );
        char* shortened = nullptr;
        if( demangled )
        {
            const auto size = strlen( demangled );
            shortened = (char*)malloc( size+1 );
            auto tmp = (char*)malloc( size+1 );

            auto dst = tmp;
            auto ptr = demangled;
            auto end = ptr + size;

            int cnt = 0;
            for(;;)
            {
                auto start = ptr;
                while( ptr < end && *ptr != '<' ) ptr++;
                memcpy( dst, start, ptr - start + 1 );
                dst += ptr - start + 1;
                if( ptr == end ) break;
                cnt++;
                ptr++;
                while( cnt > 0 )
                {
                    if( ptr == end ) break;
                    if( *ptr == '<' ) cnt++;
                    else if( *ptr == '>' ) cnt--;
                    ptr++;
                }
                *dst++ = '>';
            }

            end = dst-1;
            ptr = tmp;
            dst = shortened;
            cnt = 0;
            for(;;)
            {
                auto start = ptr;
                while( ptr < end && *ptr != '(' ) ptr++;
                memcpy( dst, start, ptr - start + 1 );
                dst += ptr - start + 1;
                if( ptr == end ) break;
                cnt++;
                ptr++;
                while( cnt > 0 )
                {
                    if( ptr == end ) break;
                    if( *ptr == '(' ) cnt++;
                    else if( *ptr == ')' ) cnt--;
                    ptr++;
                }
                *dst++ = ')';
            }

            end = dst-1;
            if( end - shortened > 6 && memcmp( end-6, " const", 6 ) == 0 )
            {
                dst[-7] = '\0';
                end -= 6;
            }

            ptr = shortened;
            for(;;)
            {
                auto match = TypesList;
                while( *match )
                {
                    auto m = *match;
                    auto p = ptr;
                    while( *m )
                    {
                        if( *m != *p ) break;
                        m++;
                        p++;
                    }
                    if( !*m )
                    {
                        ptr = p;
                        break;
                    }
                    match++;
                }
                if( !*match ) break;
            }

            free( tmp );
        }
        auto func = demangled ? shortened : function;

        if( filename )
        {
            auto path = NormalizePath( filename );
            mclog( LogLevel::Debug, "%i. %s [%s:%i]", callstackIdx++, func, path ? path : filename, lineno );
            free( path );
        }
        else
        {
            mclog( LogLevel::Debug, "%i. %s", callstackIdx++, func );
        }

        free( demangled );
        free( shortened );
    }
    else if( filename )
    {
        auto path = NormalizePath( filename );
        mclog( LogLevel::Debug, "%i. <unknown> [%s:%i]", callstackIdx++, path ? path : filename, lineno );
        free( path );
    }
    else
    {
        mclog( LogLevel::Debug, "%i. <unknown> (0x%lx)", callstackIdx++, pc );
    }
    return 0;
}

static void CallstackError( void*, const char* msg, int errnum )
{
    mclog( LogLevel::Debug, "Callstack error %i: %s", errnum, msg );
}

void PrintCallstack( const CallstackData& data )
{
    mclog( LogLevel::Debug, "Callstack:" );

    static auto state = backtrace_create_state( nullptr, 0, nullptr, nullptr );
    callstackIdx = 0;

    for( int i = 0; i < data.count; i++ )
    {
        backtrace_pcinfo( state, (uintptr_t)data.addr[i], CallstackCallback, CallstackError, nullptr );
    }
}
