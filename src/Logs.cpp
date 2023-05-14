#include <assert.h>
#include <stdio.h>

extern "C" {
#include <wlr/util/log.h>
}

#include "Logs.hpp"

void LogCallback( enum wlr_log_importance importance, const char* fmt, va_list args )
{
    switch( importance )
    {
    case WLR_SILENT: printf( "[SILENT] " ); break;
    case WLR_ERROR: printf( "[ERROR] " ); break;
    case WLR_INFO: printf( "[INFO] " ); break;
    case WLR_DEBUG: printf( "[DEBUG] " ); break;
    default: assert( false ); break;
    }

    vprintf( fmt, args );
}

void SetupLogging()
{
    wlr_log_init( WLR_DEBUG, LogCallback );
}
