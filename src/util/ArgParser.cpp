#include <algorithm>
#include <array>
#include <stdlib.h>
#include <string>
#include <string.h>

#include "ArgParser.hpp"
#include "Logs.hpp"

struct Argument
{
    constexpr Argument( const char* name, bool value )
        : name( name ), value( value ), length( std::char_traits<char>::length( name ) )
    {
    }

    const char* name;
    bool value;
    int length;
};

constexpr std::array arguments = {
    Argument { "on", true },
    Argument { "off", false },
    Argument { "true", true },
    Argument { "false", false },
    Argument { "t", true },
    Argument { "f", false },
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
    constexpr auto maxLength = std::max_element( arguments.begin(), arguments.end(), []( const auto& lhs, const auto& rhs ) { return lhs.length < rhs.length; } )->length;
    const auto length = strlen( str );
    if( length <= maxLength )
    {
        for( const auto& arg : arguments )
        {
            if( length == arg.length && strcmp( str, arg.name ) == 0 ) return arg.value;
        }
    }
    mclog( LogLevel::Error, "Argument '%s' is not a valid boolean", str );
    exit( 1 );
}
