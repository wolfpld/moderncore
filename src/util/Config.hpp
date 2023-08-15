#pragma once

#include "NoCopy.hpp"

extern "C" {
    struct ini_t;
}

class Config
{
public:
    explicit Config( const char* name );
    ~Config();

    NoCopy( Config );

    template<typename T>
    T Get( const char* section, const char* key, T defaultVal );

private:
    const char* GetString( const char* section, const char* key, const char* def );
    int GetInt( const char* section, const char* key, int def );

    ini_t* m_config;
};

template<>
inline const char* Config::Get( const char* s, const char* k, const char* v ) { return GetString( s, k, v ); }

template<>
inline int Config::Get( const char* s, const char* k, int v ) { return GetInt( s, k, v ); }
