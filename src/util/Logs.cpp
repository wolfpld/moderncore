#include <assert.h>
#include <mutex>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <tracy/Tracy.hpp>

#include "Ansi.hpp"
#include "Callstack.hpp"
#include "Logs.hpp"

namespace
{
LogLevel s_logLevel = LogLevel::Info;
bool s_logSynchronized = false;
std::recursive_mutex s_logLock;
}

void SetLogLevel( LogLevel level )
{
    s_logLevel = level;
}

void SetLogSynchronized( bool sync )
{
    s_logSynchronized = sync;
}

static void PrintLevel( LogLevel level )
{
    switch( level )
    {
    case LogLevel::Callstack: printf( ANSI_CYAN "[STACK] " ); break;
    case LogLevel::Debug: printf( ANSI_BOLD ANSI_BLACK "[DEBUG] " ); break;
    case LogLevel::Info: printf( " [INFO] " ); break;
    case LogLevel::Warning: printf( ANSI_BOLD ANSI_YELLOW " [WARN] " ); break;
    case LogLevel::Error: printf( ANSI_BOLD ANSI_RED "[ERROR] " ); break;
    case LogLevel::Fatal: printf( ANSI_BOLD ANSI_MAGENTA "[FATAL] " ); break;
    default: assert( false ); break;
    }
}

void MCoreLogMessage( LogLevel level, const char* fileName, size_t line, const char *fmt, ... )
{
    if( level < s_logLevel ) return;

    va_list args;
    va_start( args, fmt );

    constexpr int FnLen = 20;
    const auto len = strlen( fileName );

    // Get callstack outside of lock
    const bool printCallstack = level >= LogLevel::Error && s_logLevel <= LogLevel::Callstack;
    CallstackData stack;
    if( printCallstack ) stack.count = backtrace( stack.addr, 64 );

    s_logLock.lock();
    PrintLevel( level );
    if( len > FnLen )
    {
        printf( "…%s:%-4zu| ", fileName + len - FnLen - 1, line );
    }
    else
    {
        printf( "%*s:%-4zu| ", FnLen, fileName, line );
    }
    vprintf( fmt, args );
    printf( ANSI_RESET "\n" );
    fflush( stdout );
    if( printCallstack ) PrintCallstack( stack, 1 );
    s_logLock.unlock();

    va_end( args );

#ifdef TRACY_ENABLE
    if( level != LogLevel::Callstack )
    {
        va_start( args, fmt );
        char tmp[8*1024];
        const auto res = vsnprintf( tmp, sizeof( tmp ), fmt, args );
        if( res > 0 )
        {
            switch( level )
            {
            case LogLevel::Debug: TracyMessageC( tmp, res, 0x888888 ); break;
            case LogLevel::Info: TracyMessage( tmp, res ); break;
            case LogLevel::Warning: TracyMessageC( tmp, res, 0xFFFF00 ); break;
            case LogLevel::Error: TracyMessageCS( tmp, res, 0xFF0000, 64 ); break;
            case LogLevel::Fatal: TracyMessageCS( tmp, res, 0xFF00FF, 64 ); break;
            default: assert( false ); break;
            }
        }
        va_end( args );
    }
#endif
}
