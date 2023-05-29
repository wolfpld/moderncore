#include <stdlib.h>
#include <string>

#include "Config.hpp"
#include "../util/Home.hpp"

extern "C" {
#include "../../contrib/ini/ini.h"
}

Config::Config( const char* name )
    : m_config( nullptr )
{
    std::string path;
    const auto xdgConfig = getenv( "XDG_CONFIG_HOME" );
    if( xdgConfig )
    {
        path = xdgConfig;
        path += "/ModernCore/";
    }
    else
    {
        path = GetHome();
        path += "/.config/ModernCore/";
    }
    path += name;

    m_config = ini_load( path.c_str() );
}

Config::~Config()
{
    if( m_config ) ini_free( m_config );
}

const char* Config::GetString( const char* section, const char* key, const char* def )
{
    if( !m_config ) return def;
    auto val = ini_get( m_config, section, key );
    return val ? val : def;
}

int Config::GetInt( const char* section, const char* key, int def )
{
    if( !m_config ) return def;
    auto val = ini_get( m_config, section, key );
    if( !val ) return def;
    char* end;
    auto num = strtol( val, &end, 10 );
    return (end == val) ? def : num;
}
