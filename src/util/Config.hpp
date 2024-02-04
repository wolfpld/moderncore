#pragma once

#include <string>

#include "NoCopy.hpp"

extern "C" {
    struct ini_t;
}

class Config
{
public:
    explicit Config( const char* name );
    explicit Config( const std::string& name ) : Config( name.c_str() ) {}
    ~Config();

    NoCopy( Config );

    explicit operator bool() const { return m_config; }

    template<typename T>
    T Get( const char* section, const char* key, T defaultVal );

    // "Optional"
    template<typename T>
    bool GetOpt( const char* section, const char* key, T& output );

private:
    const char* GetString( const char* section, const char* key, const char* def );
    int GetInt( const char* section, const char* key, int def );

    bool GetOptString( const char* section, const char* key, const char*& output );
    bool GetOptInt( const char* section, const char* key, int& output );

    ini_t* m_config;
};

template<>
inline const char* Config::Get( const char* s, const char* k, const char* v ) { return GetString( s, k, v ); }

template<>
inline int Config::Get( const char* s, const char* k, int v ) { return GetInt( s, k, v ); }

template<>
inline bool Config::GetOpt( const char* s, const char* k, const char*& v ) { return GetOptString( s, k, v ); }

template<>
inline bool Config::GetOpt( const char* s, const char* k, int& v ) { return GetOptInt( s, k, v ); }
