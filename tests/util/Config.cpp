#include <catch2/catch_all.hpp>
#include <src/util/Config.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Helper to create a temp INI file with given content in current directory
static std::string createTempIniFile( const char* content )
{
    char filename[256];
    snprintf( filename, sizeof( filename ), "./test_config_%d_%ld.ini", getpid(), (long)time( nullptr ) );

    FILE* f = fopen( filename, "w" );
    if( !f ) return "";

    if( content )
    {
        fprintf( f, "%s", content );
    }

    fclose( f );
    return std::string( filename );
}

TEST_CASE( "Config functionality", "[config][ini]" )
{
    SECTION( "Constructor with valid INI file" )
    {
        std::string tempPath = createTempIniFile(
            "[test_section]\n"
            "string_key=test_value\n"
            "int_key=42\n"
            "uint_key=256" );
        REQUIRE( !tempPath.empty() );

        Config config( tempPath.c_str() );
        REQUIRE( (bool)config );

        unlink( tempPath.c_str() );
    }

    SECTION( "Constructor with invalid file path" )
    {
        Config config( "nonexistent_test_config.ini" );
        REQUIRE( !(bool)config );
    }

    SECTION( "Get with existing keys" )
    {
        std::string tempPath = createTempIniFile(
            "[test_section]\n"
            "string_key=test_value\n"
            "int_key=42\n"
            "uint_key=256" );
        REQUIRE( !tempPath.empty() );

        Config config( tempPath.c_str() );
        auto strVal = config.Get( "test_section", "string_key", "default" );
        auto intVal = config.Get( "test_section", "int_key", 0 );
        auto uintVal = config.Get( "test_section", "uint_key", (uint32_t)0 );

        REQUIRE( strcmp( strVal, "test_value" ) == 0 );
        REQUIRE( intVal == 42 );
        REQUIRE( uintVal == 256 );

        unlink( tempPath.c_str() );
    }

    SECTION( "Get with missing keys returns defaults" )
    {
        std::string tempPath = createTempIniFile(
            "[test_section]\n"
            "existing_key=value" );
        REQUIRE( !tempPath.empty() );

        Config config( tempPath.c_str() );
        auto defaultStr = config.Get( "test_section", "missing_string_key", "default" );
        auto defaultInt = config.Get( "test_section", "missing_int_key", 999 );
        auto defaultUint = config.Get( "test_section", "missing_uint_key", (uint32_t)999 );

        REQUIRE( strcmp( defaultStr, "default" ) == 0 );
        REQUIRE( defaultInt == 999 );
        REQUIRE( defaultUint == 999 );

        unlink( tempPath.c_str() );
    }

    SECTION( "GetOpt with existing keys" )
    {
        std::string tempPath = createTempIniFile(
            "[test_section]\n"
            "string_key=test_value" );
        REQUIRE( !tempPath.empty() );

        Config config( tempPath.c_str() );
        const char* output;
        bool result = config.GetOpt( "test_section", "string_key", output );
        REQUIRE( result == true );
        REQUIRE( output != nullptr );

        unlink( tempPath.c_str() );
    }

    SECTION( "GetOpt with missing keys" )
    {
        std::string tempPath = createTempIniFile(
            "[test_section]\n"
            "existing_key=value" );
        REQUIRE( !tempPath.empty() );

        Config config( tempPath.c_str() );
        const char* output;
        bool result = config.GetOpt( "test_section", "missing_key", output );
        REQUIRE( result == false );

        unlink( tempPath.c_str() );
    }

    SECTION( "GetPath with ./ prefix" )
    {
        std::string path = Config::GetPath( "./test.ini" );
        REQUIRE( path == "./test.ini" );
    }

    SECTION( "GetPath with environment variable" )
    {
        const char* originalXdgConfig = getenv( "XDG_CONFIG_HOME" );

        if( originalXdgConfig )
        {
            unsetenv( "XDG_CONFIG_HOME" );
        }

        std::string path = Config::GetPath( "test.ini" );
        REQUIRE( path.find( "ModernCore/test.ini" ) != std::string::npos );

        setenv( "XDG_CONFIG_HOME", "/tmp/test_config", 1 );
        std::string pathXdg = Config::GetPath( "test.ini" );
        REQUIRE( pathXdg == "/tmp/test_config/ModernCore/test.ini" );

        if( originalXdgConfig )
        {
            setenv( "XDG_CONFIG_HOME", originalXdgConfig, 1 );
        }
    }
}
