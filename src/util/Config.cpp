#include <stdlib.h>
#include <string>
#include <string.h>

#include "Config.hpp"
#include "util/Home.hpp"

#include "contrib/ini/ini.h"

Config::Config( const char* name )
    : m_config( nullptr )
{
    if( strncmp( name, "./", 2 ) != 0 )
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
    else
    {
        m_config = ini_load( name );
    }
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

bool Config::GetOptString( const char* section, const char* key, const char*& output )
{
    if( !m_config ) return false;
    output = ini_get( m_config, section, key );
    return output != nullptr;
}

bool Config::GetOptInt( const char* section, const char* key, int& output )
{
    if( !m_config ) return false;
    auto val = ini_get( m_config, section, key );
    if( !val ) return false;
    char* end;
    output = strtol( val, &end, 10 );
    return (end != val);
}
