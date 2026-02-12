#include <catch2/catch_all.hpp>
#include <src/util/Config.hpp>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

TEST_CASE( "Config functionality", "[config][ini]" )
{
    SECTION( "Constructor with valid INI file" )
    {
        char tempPath[256];
        snprintf( tempPath, sizeof( tempPath ), "./test_config_valid_%d.ini", getpid() );

        FILE* f = fopen( tempPath, "w" );
        REQUIRE( f != nullptr );
        fprintf( f, "[test_section]\n"
                    "string_key=test_value\n"
                    "int_key=42\n"
                    "uint_key=256" );
        fclose( f );

        Config config( tempPath );
        REQUIRE( (bool)config );

        unlink( tempPath );
    }

    SECTION( "Constructor with invalid file path" )
    {
        Config config( "nonexistent_test_config.ini" );
        REQUIRE( !(bool)config );
    }

    SECTION( "Get with existing keys" )
    {
        char tempPath[256];
        snprintf( tempPath, sizeof( tempPath ), "./test_config_get_%d.ini", getpid() );

        FILE* f = fopen( tempPath, "w" );
        REQUIRE( f != nullptr );
        fprintf( f, "[test_section]\n"
                    "string_key=test_value\n"
                    "int_key=42\n"
                    "uint_key=256" );
        fclose( f );

        Config config( tempPath );
        auto strVal = config.Get( "test_section", "string_key", "default" );
        auto intVal = config.Get( "test_section", "int_key", 0 );
        auto uintVal = config.Get( "test_section", "uint_key", (uint32_t)0 );

        REQUIRE( strcmp( strVal, "test_value" ) == 0 );
        REQUIRE( intVal == 42 );
        REQUIRE( uintVal == 256 );

        unlink( tempPath );
    }

    SECTION( "Get with missing keys returns defaults" )
    {
        char tempPath[256];
        snprintf( tempPath, sizeof( tempPath ), "./test_config_missing_%d.ini", getpid() );

        FILE* f = fopen( tempPath, "w" );
        REQUIRE( f != nullptr );
        fprintf( f, "[test_section]\n"
                    "existing_key=value" );
        fclose( f );

        Config config( tempPath );
        auto defaultStr = config.Get( "test_section", "missing_string_key", "default" );
        auto defaultInt = config.Get( "test_section", "missing_int_key", 999 );
        auto defaultUint = config.Get( "test_section", "missing_uint_key", (uint32_t)999 );

        REQUIRE( strcmp( defaultStr, "default" ) == 0 );
        REQUIRE( defaultInt == 999 );
        REQUIRE( defaultUint == 999 );

        unlink( tempPath );
    }

    SECTION( "GetOpt with existing keys" )
    {
        char tempPath[256];
        snprintf( tempPath, sizeof( tempPath ), "./test_config_getopt_%d.ini", getpid() );

        FILE* f = fopen( tempPath, "w" );
        REQUIRE( f != nullptr );
        fprintf( f, "[test_section]\n"
                    "string_key=test_value" );
        fclose( f );

        Config config( tempPath );
        const char* output;
        bool result = config.GetOpt( "test_section", "string_key", output );
        REQUIRE( result == true );
        REQUIRE( output != nullptr );

        unlink( tempPath );
    }

    SECTION( "GetOpt with missing keys" )
    {
        char tempPath[256];
        snprintf( tempPath, sizeof( tempPath ), "./test_config_getopt_missing_%d.ini", getpid() );

        FILE* f = fopen( tempPath, "w" );
        REQUIRE( f != nullptr );
        fprintf( f, "[test_section]\n"
                    "existing_key=value" );
        fclose( f );

        Config config( tempPath );
        const char* output;
        bool result = config.GetOpt( "test_section", "missing_key", output );
        REQUIRE( result == false );

        unlink( tempPath );
    }

    SECTION( "GetPath with ./ prefix" )
    {
        std::string path = Config::GetPath( "./test.ini" );
        REQUIRE( path == "./test.ini" );
    }

    SECTION( "GetPath with environment variable" )
    {
        // Save original environment variable
        const char* originalXdgConfig = getenv( "XDG_CONFIG_HOME" );

        // Test with XDG_CONFIG_HOME unset
        if( originalXdgConfig )
        {
            unsetenv( "XDG_CONFIG_HOME" );
        }

        std::string path = Config::GetPath( "test.ini" );
        REQUIRE( path.find( "ModernCore/test.ini" ) != std::string::npos );

        // Test with XDG_CONFIG_HOME set
        setenv( "XDG_CONFIG_HOME", "/tmp/test_config", 1 );
        std::string pathXdg = Config::GetPath( "test.ini" );
        REQUIRE( pathXdg == "/tmp/test_config/ModernCore/test.ini" );

        // Restore original environment variable
        if( originalXdgConfig )
        {
            setenv( "XDG_CONFIG_HOME", originalXdgConfig, 1 );
        }
    }
}
