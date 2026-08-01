#include <cxxabi.h>
#include <format>
#include <stdint.h>
#include <string>
#include <string.h>

#include "Callstack.hpp"
#include "Logs.hpp"
#include "PathNormalize.hpp"

static int callstackIdx;
static bool callstackExternal;
static bool callstackShowExternal = false;

#ifndef DISABLE_CALLSTACK

#include "contrib/libbacktrace/backtrace.h"

constexpr const char* TypesList[] = {
    "bool ", "char ", "double ", "float ", "int ", "long ", "short ",
    "signed ", "unsigned ", "void ", "wchar_t ", "size_t ", "int8_t ",
    "int16_t ", "int32_t ", "int64_t ", "intptr_t ", "uint8_t ", "uint16_t ",
    "uint32_t ", "uint64_t ", "ptrdiff_t ", nullptr
};

static bool IsPathExternal( const char* path )
{
    return strncmp( path, "/usr/", 5 ) == 0 || strncmp( path, "/lib/", 5 ) == 0;
}

static int CallstackCallback( void*, uintptr_t pc, const char* filename, int lineno, const char* function )
{
    bool isExternal = false;
    std::string msg;

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
            if( !path.empty() && IsPathExternal( path.c_str() ) )
            {
                isExternal = true;
                callstackExternal = true;
            }
            const auto* shown = path.empty() ? filename : path.c_str();
            if( lineno )
            {
                msg = std::format( "{}. {} [{}:{}]", callstackIdx++, func, shown, lineno );
            }
            else
            {
                msg = std::format( "{}. {} [{}]", callstackIdx++, func, shown );
            }
        }
        else
        {
            msg = std::format( "{}. {}", callstackIdx++, func );
        }

        free( demangled );
        free( shortened );
    }
    else if( filename )
    {
        auto path = NormalizePath( filename );
        if( !path.empty() && IsPathExternal( path.c_str() ) )
        {
            isExternal = true;
            callstackExternal = true;
        }
        const auto* shown = path.empty() ? filename : path.c_str();
        if( lineno )
        {
            msg = std::format( "{}. <unknown> [{}:{}]", callstackIdx++, shown, lineno );
        }
        else
        {
            msg = std::format( "{}. <unknown> [{}]", callstackIdx++, shown );
        }
    }
    else
    {
        isExternal = true;
        callstackExternal = true;
        msg = std::format( "{}. <unknown> (0x{:x})", callstackIdx++, pc );
    }

    if( callstackShowExternal )
    {
        mclog( LogLevel::Callstack, "%s", msg.c_str() );
    }
    else if( !isExternal )
    {
        if( callstackExternal )
        {
            mclog( LogLevel::Callstack, "…" );
            callstackExternal = false;
        }
        mclog( LogLevel::Callstack, "%s", msg.c_str() );
    }

    return 0;
}

static void CallstackError( void*, const char* msg, int errnum )
{
    mclog( LogLevel::Callstack, "Callstack error %i: %s", errnum, msg );
}

void PrintCallstack( const CallstackData& data, int skip )
{
    mclog( LogLevel::Callstack, "Callstack:" );

    static auto state = backtrace_create_state( nullptr, 0, nullptr, nullptr );
    callstackIdx = 0;
    callstackExternal = false;

    for( int i = skip; i < data.count; i++ )
    {
        backtrace_pcinfo( state, (uintptr_t)data.addr[i], CallstackCallback, CallstackError, nullptr );
    }
}

#else

void PrintCallstack( const CallstackData& data, int skip )
{
}

#endif

void ShowExternalCallstacks( bool show )
{
    callstackShowExternal = show;
}
