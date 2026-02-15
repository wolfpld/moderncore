#include "TestUtils.hpp"
#include <catch2/catch_all.hpp>
#include <fcntl.h>
#include <src/util/Config.hpp>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static TempDir setupConfigDir()
{
    TempDir configDir = TempDir::create();
    configDir.createSubdir( "ModernCore" );
    setenv( "XDG_CONFIG_HOME", configDir.path(), 1 );
    return configDir;
}

static std::string writeConfigFile( const TempDir& dir, const char* name, const char* content )
{
    std::string modernCoreDir = std::string( dir.path() ) + "/ModernCore";
    std::string fullPath = modernCoreDir + "/" + name;

    int fd = open( fullPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644 );
    REQUIRE( fd >= 0 );

    if( content && strlen( content ) > 0 )
    {
        ssize_t written = write( fd, content, strlen( content ) );
        REQUIRE( written == static_cast<ssize_t>( strlen( content ) ) );
    }

    close( fd );
    return fullPath;
}

TEST_CASE( "Config functionality", "[config][ini]" )
{
    const char* originalXdgConfig = getenv( "XDG_CONFIG_HOME" );
    TempDir configDir = setupConfigDir();

    SECTION( "Constructor with valid INI file" )
    {
        writeConfigFile( configDir, "test.ini", "[test_section]\nstring_key=test_value\nint_key=42\nuint_key=256" );

        Config config( "test.ini" );
        REQUIRE( (bool)config );
    }

    SECTION( "Constructor with invalid file path" )
    {
        Config config( "nonexistent_test_config.ini" );
        REQUIRE( !(bool)config );
    }

    SECTION( "Get with existing keys" )
    {
        writeConfigFile( configDir, "test.ini", "[test_section]\nstring_key=test_value\nint_key=42\nuint_key=256" );

        Config config( "test.ini" );
        auto strVal = config.Get( "test_section", "string_key", "default" );
        auto intVal = config.Get( "test_section", "int_key", 0 );
        auto uintVal = config.Get( "test_section", "uint_key", (uint32_t)0 );

        REQUIRE( strcmp( strVal, "test_value" ) == 0 );
        REQUIRE( intVal == 42 );
        REQUIRE( uintVal == 256 );
    }

    SECTION( "Get with missing keys returns defaults" )
    {
        writeConfigFile( configDir, "test.ini", "[test_section]\nexisting_key=value" );

        Config config( "test.ini" );
        auto defaultStr = config.Get( "test_section", "missing_string_key", "default" );
        auto defaultInt = config.Get( "test_section", "missing_int_key", 999 );
        auto defaultUint = config.Get( "test_section", "missing_uint_key", (uint32_t)999 );

        REQUIRE( strcmp( defaultStr, "default" ) == 0 );
        REQUIRE( defaultInt == 999 );
        REQUIRE( defaultUint == 999 );
    }

    SECTION( "GetOpt with existing keys" )
    {
        writeConfigFile( configDir, "test.ini", "[test_section]\nstring_key=test_value" );

        Config config( "test.ini" );
        const char* output;
        bool result = config.GetOpt( "test_section", "string_key", output );
        REQUIRE( result == true );
        REQUIRE( output != nullptr );
    }

    SECTION( "GetOpt with missing keys" )
    {
        writeConfigFile( configDir, "test.ini", "[test_section]\nexisting_key=value" );

        Config config( "test.ini" );
        const char* output;
        bool result = config.GetOpt( "test_section", "missing_key", output );
        REQUIRE( result == false );
    }

    SECTION( "GetPath with ./ prefix" )
    {
        std::string path = Config::GetPath( "./test.ini" );
        REQUIRE( path == "./test.ini" );
    }

    SECTION( "GetPath with environment variable" )
    {
        std::string path = Config::GetPath( "test.ini" );
        REQUIRE( path == std::string( configDir.path() ) + "/ModernCore/test.ini" );
    }

    if( originalXdgConfig )
    {
        setenv( "XDG_CONFIG_HOME", originalXdgConfig, 1 );
    }
    else
    {
        unsetenv( "XDG_CONFIG_HOME" );
    }
}