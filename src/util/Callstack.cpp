#include <cxxabi.h>
#include <stdint.h>

#include "Callstack.hpp"
#include "Logs.hpp"
#include "../../contrib/libbacktrace/backtrace.h"

static int callstackIdx;

static int CallstackCallback( void*, uintptr_t pc, const char* filename, int lineno, const char* function )
{
    if( function )
    {
        auto demangled = abi::__cxa_demangle( function, nullptr, nullptr, nullptr );
        auto func = demangled ? demangled : function;

        if( filename )
        {
            mclog( LogLevel::Debug, "%i. %s [%s:%i]", callstackIdx++, func, filename, lineno );
        }
        else
        {
            mclog( LogLevel::Debug, "%i. %s", callstackIdx++, func );
        }

        if( demangled ) free( demangled );
    }
    else if( filename )
    {
        mclog( LogLevel::Debug, "%i. <unknown> [%s:%i]", callstackIdx++, filename, lineno );
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
