#include <cxxabi.h>
#include <format>
#include <stdint.h>
#include <string>
#include <string.h>
#include <string_view>

#include "Callstack.hpp"
#include "Logs.hpp"
#include "PathNormalize.hpp"

static int callstackIdx;
static bool callstackExternal;
static bool callstackShowExternal = false;

#ifndef DISABLE_CALLSTACK

#include "contrib/libbacktrace/backtrace.h"

static bool IsPathExternal( const char* path )
{
    return strncmp( path, "/usr/", 5 ) == 0 || strncmp( path, "/lib/", 5 ) == 0;
}

static bool IsOperatorBracket( std::string_view name, size_t i )
{
    size_t run = 0;
    while( i > 0 && name[i-1] == '<' && run < 2 )
    {
        i--;
        run++;
    }
    if( run > 1 ) return false;
    if( i < 8 || name.substr( i-8, 8 ) != "operator" ) return false;
    return i == 8 || name[i-9] == ' ' || name[i-9] == ':';
}

static std::string ShortenFunctionName( std::string_view name )
{
    std::string out;
    out.reserve( name.size() );
    for( size_t i = 0; i < name.size(); )
    {
        const char c = name[i];
        if( c != '<' && c != '(' )
        {
            out += c;
            i++;
            continue;
        }
        if( c == '<' && IsOperatorBracket( name, i ) )
        {
            out += c;
            i++;
            continue;
        }
        const char close = c == '<' ? '>' : ')';
        int depth = 0;
        size_t j = i;
        for( ; j < name.size(); j++ )
        {
            if( name[j] == c ) depth++;
            else if( name[j] == close && --depth == 0 )
            {
                j++;
                break;
            }
        }
        out += c;
        out += close;
        i = j;
    }
    for(;;)
    {
        if( out.ends_with( " const" ) ) out.resize( out.size() - 6 );
        else if( out.ends_with( " volatile" ) || out.ends_with( " noexcept" ) ) out.resize( out.size() - 9 );
        else break;
    }

    const size_t params = out.rfind( '(' );
    if( params != std::string::npos )
    {
        for( size_t p = out.rfind( "operator ", params ); p != std::string::npos;
             p = p > 0 ? out.rfind( "operator ", p - 1 ) : std::string::npos )
        {
            if( p > 0 && out[p-1] != ' ' && out[p-1] != ':' ) continue;
            const std::string_view tail( out.data() + p + 9, params - ( p + 9 ) );
            if( !tail.starts_with( "new" ) && !tail.starts_with( "delete" ) && !tail.starts_with( "\"\"" ) )
            {
                return out;
            }
            break;
        }
    }
    for( size_t params2 = out.rfind( '(' ); params2 != std::string::npos; )
    {
        size_t s = out.rfind( ' ', params2 );
        bool erased = false;
        while( s != std::string::npos )
        {
            const size_t prev = s > 0 ? out.rfind( ' ', s - 1 ) : std::string::npos;
            const size_t wordStart = prev == std::string::npos ? 0 : prev + 1;
            const std::string_view word( out.data() + wordStart, s - wordStart );
            std::string_view stem = word;
            while( !stem.empty() && strchr( "+-*/%^&|~!<>=[](){}\"", stem.back() ) ) stem.remove_suffix( 1 );
            const bool operatorSpace = ( stem == "operator" ) ||
                ( stem.size() > 8 && stem.ends_with( "operator" ) && stem[stem.size() - 9] == ':' );
            if( !operatorSpace )
            {
                out.erase( 0, s + 1 );
                erased = true;
                break;
            }
            if( prev == std::string::npos ) break;
            s = prev;
        }
        if( !erased ) break;
        params2 = out.rfind( '(' );
    }
    return out;
}

static int CallstackCallback( void*, uintptr_t pc, const char* filename, int lineno, const char* function )
{
    bool isExternal = false;
    std::string msg;

    if( function )
    {
        auto demangled = abi::__cxa_demangle( function, nullptr, nullptr, nullptr );
        std::string shortened;
        if( demangled )
        {
            shortened = ShortenFunctionName( demangled );
        }
        const char* func = demangled ? shortened.c_str() : function;

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
