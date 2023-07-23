#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#include "Logs.hpp"
#include "../util/Ansi.hpp"

static void PrintLevel( LogLevel level )
{
    switch( level )
    {
    case LogLevel::Debug: printf( ANSI_BOLD ANSI_BLACK "[DEBUG] " ); break;
    case LogLevel::Info: printf( "[INFO] " ); break;
    case LogLevel::Warning: printf( ANSI_BOLD ANSI_YELLOW "[WARNING] " ); break;
    case LogLevel::Error: printf( ANSI_BOLD ANSI_RED "[ERROR] " ); break;
    case LogLevel::Fatal: printf( ANSI_BOLD ANSI_MAGENTA "[FATAL] " ); break;
    default: assert( false ); break;
    }
}

void MCoreLogMessage( LogLevel level, const char* fileName, size_t line, const char *fmt, ... )
{
    va_list args;
    va_start( args, fmt );

    PrintLevel( level );
    printf( "[%s:%zu] ", fileName, line );
    vprintf( fmt, args );
    printf( ANSI_RESET "\n" );
    fflush( stdout );

    va_end( args );
}

void MCoreLogMessage( LogLevel level, const char* fileName, size_t line, const std::string& str )
{
    PrintLevel( level );
    printf( "[%s:%zu] %s" ANSI_RESET "\n", fileName, line, str.c_str() );
    fflush( stdout );
}
