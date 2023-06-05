#include <assert.h>
#include <stdio.h>

extern "C" {
#include <wlr/util/log.h>
}

#include "Logs.hpp"
#include "../util/Ansi.hpp"

void LogCallback( wlr_log_importance importance, const char* fmt, va_list args )
{
    switch( importance )
    {
    case WLR_SILENT: printf( "[SILENT] " ); break;
    case WLR_ERROR: printf( ANSI_BOLD ANSI_RED "[ERROR] " ); break;
    case WLR_INFO: printf( "[INFO] " ); break;
    case WLR_DEBUG: printf( ANSI_BOLD ANSI_BLACK "[DEBUG] " ); break;
    default: assert( false ); break;
    }

    vprintf( fmt, args );
    printf( ANSI_RESET "\n" );
    fflush( stdout );
}

void SetupLogging()
{
    wlr_log_init( WLR_DEBUG, LogCallback );
}

void MCoreLogMessage( LogLevel level, const char* fileName, size_t line, const char *fmt, ... )
{
    va_list args;
    va_start( args, fmt );

    switch( level )
    {
    case LogLevel::Debug: printf( ANSI_BOLD ANSI_BLACK "[DEBUG] " ); break;
    case LogLevel::Info: printf( "[INFO] " ); break;
    case LogLevel::Warning: printf( ANSI_BOLD ANSI_YELLOW "[WARNING] " ); break;
    case LogLevel::Error: printf( ANSI_BOLD ANSI_RED "[ERROR] " ); break;
    case LogLevel::Fatal: printf( ANSI_BOLD ANSI_MAGENTA "[FATAL] " ); break;
    default: assert( false ); break;
    }

    printf( "[%s:%zu] ", fileName, line );
    vprintf( fmt, args );
    printf( ANSI_RESET "\n" );
    fflush( stdout );
    va_end( args );
}
