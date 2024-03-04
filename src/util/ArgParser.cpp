#include <array>
#include <stdlib.h>
#include <string.h>

#include "ArgParser.hpp"
#include "Logs.hpp"

struct Argument
{
    const char* name;
    bool value;
};

constexpr std::array arguments = {
    Argument { "on", true },
    Argument { "off", false },
    Argument { "true", true },
    Argument { "false", false },
    Argument { "1", true },
    Argument { "0", false },
    Argument { "yes", true },
    Argument { "no", false },
    Argument { "y", true },
    Argument { "n", false },
    Argument { "enable", true },
    Argument { "disable", false },
    Argument { "enabled", true },
    Argument { "disabled", false },
    Argument { "active", true },
    Argument { "inactive", false }
};

bool ParseBoolean( const char* str )
{
    for( const auto& arg : arguments )
    {
        if( strcmp( str, arg.name ) == 0 ) return arg.value;
    }
    mclog( LogLevel::Error, "Argument '%s' is not a valid boolean", str );
    exit( 1 );
}
