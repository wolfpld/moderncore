#include <assert.h>
#include <stdio.h>

extern "C" {
#include <wlr/util/log.h>
}

#include "Ansi.hpp"
#include "Logs.hpp"

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
}

void SetupLogging()
{
    wlr_log_init( WLR_DEBUG, LogCallback );
}
