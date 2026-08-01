#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string>
#include <string.h>

#include "Config.hpp"
#include "util/Home.hpp"

#include "contrib/ini/ini.h"

Config::Config( const char* name )
    : m_config( ini_load( GetPath( name ).c_str() ) )
{
}

Config::~Config()
{
    if( m_config ) ini_free( m_config );
}

std::string Config::GetPath( const char* name )
{
    if( strncmp( name, "./", 2 ) == 0 ) return name;

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

    return path;
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
    errno = 0;
    char* end;
    auto num = strtol( val, &end, 10 );
    if( end == val || errno == ERANGE || num < INT_MIN || num > INT_MAX ) return def;
    return (int)num;
}

uint32_t Config::GetUInt( const char* section, const char* key, uint32_t def )
{
    if( !m_config ) return def;
    auto val = ini_get( m_config, section, key );
    if( !val ) return def;
    errno = 0;
    char* end;
    auto num = strtoll( val, &end, 10 );
    if( end == val || errno == ERANGE || num < 0 || num > UINT32_MAX ) return def;
    return (uint32_t)num;
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
    errno = 0;
    char* end;
    auto num = strtol( val, &end, 10 );
    if( end == val || errno == ERANGE || num < INT_MIN || num > INT_MAX ) return false;
    output = (int)num;
    return true;
}

bool Config::GetOptUInt( const char* section, const char* key, uint32_t& output )
{
    if( !m_config ) return false;
    auto val = ini_get( m_config, section, key );
    if( !val ) return false;
    errno = 0;
    char* end;
    auto num = strtoll( val, &end, 10 );
    if( end == val || errno == ERANGE || num < 0 || num > UINT32_MAX ) return false;
    output = (uint32_t)num;
    return true;
}
