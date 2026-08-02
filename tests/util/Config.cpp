#include "TestUtils.hpp"
#include <catch2/catch_all.hpp>
#include <fcntl.h>
#include <src/util/Config.hpp>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static TempDir createConfigDir()
{
    TempDir configDir = TempDir::create();
    configDir.createSubdir( "ModernCore" );
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
    EnvGuard xdgGuard( "XDG_CONFIG_HOME" );
    TempDir configDir = createConfigDir();
    xdgGuard.Set( configDir.path() );

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

    SECTION( "Constructor with std::string" )
    {
        writeConfigFile( configDir, "string.ini", "[section]\nkey=value" );

        Config config( std::string( "string.ini" ) );
        REQUIRE( (bool)config );
        REQUIRE( strcmp( config.Get( "section", "key", "" ), "value" ) == 0 );
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

    SECTION( "Get with invalid numeric values returns defaults" )
    {
        writeConfigFile( configDir, "test.ini", "[test_section]\nneg_uint_key=-1\novf_uint_key=999999999999999999999\novf_int_key=999999999999999999999\ngarbage_int_key=abc" );

        Config config( "test.ini" );
        auto negUint = config.Get( "test_section", "neg_uint_key", (uint32_t)7 );
        auto ovfUint = config.Get( "test_section", "ovf_uint_key", (uint32_t)7 );
        auto ovfInt = config.Get( "test_section", "ovf_int_key", 7 );
        auto garbageInt = config.Get( "test_section", "garbage_int_key", 7 );

        REQUIRE( negUint == 7 );
        REQUIRE( ovfUint == 7 );
        REQUIRE( ovfInt == 7 );
        REQUIRE( garbageInt == 7 );
    }

    SECTION( "GetOpt with invalid numeric values returns false" )
    {
        writeConfigFile( configDir, "test.ini", "[test_section]\nneg_uint_key=-1\novf_uint_key=999999999999999999999\novf_int_key=999999999999999999999\ngarbage_int_key=abc" );

        Config config( "test.ini" );
        uint32_t uintOut = 0;
        int intOut = 0;

        REQUIRE( !config.GetOpt( "test_section", "neg_uint_key", uintOut ) );
        REQUIRE( !config.GetOpt( "test_section", "ovf_uint_key", uintOut ) );
        REQUIRE( !config.GetOpt( "test_section", "ovf_int_key", intOut ) );
        REQUIRE( !config.GetOpt( "test_section", "garbage_int_key", intOut ) );
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

}

TEST_CASE( "Config GetOpt with invalid config", "[config][ini]" )
{
    Config config( "nonexistent_config_for_test.ini" );
    REQUIRE( !(bool)config );

    const char* strOut = "unused";
    int intOut = 0;
    uint32_t uintOut = 0;

    REQUIRE( !config.GetOpt( "section", "key", strOut ) );
    REQUIRE( !config.GetOpt( "section", "key", intOut ) );
    REQUIRE( !config.GetOpt( "section", "key", uintOut ) );

    REQUIRE( std::string( strOut ) == "unused" );
    REQUIRE( intOut == 0 );
    REQUIRE( uintOut == 0 );
}

TEST_CASE( "Config GetOpt valid numeric keys", "[config][ini]" )
{
    EnvGuard xdgGuard( "XDG_CONFIG_HOME" );
    TempDir configDir = createConfigDir();
    xdgGuard.Set( configDir.path() );
    writeConfigFile( configDir, "test.ini", "[test_section]\nint_key=42\nuint_key=256" );

    Config config( "test.ini" );
    int intOut = 0;
    uint32_t uintOut = 0;

    REQUIRE( config.GetOpt( "test_section", "int_key", intOut ) );
    REQUIRE( config.GetOpt( "test_section", "uint_key", uintOut ) );

    REQUIRE( intOut == 42 );
    REQUIRE( uintOut == 256 );
}

TEST_CASE( "Config GetOpt missing numeric keys", "[config][ini]" )
{
    EnvGuard xdgGuard( "XDG_CONFIG_HOME" );
    TempDir configDir = createConfigDir();
    xdgGuard.Set( configDir.path() );
    writeConfigFile( configDir, "test.ini", "[test_section]\nexisting_key=value" );

    Config config( "test.ini" );
    int intOut = 0;
    uint32_t uintOut = 0;

    REQUIRE( !config.GetOpt( "test_section", "missing_int_key", intOut ) );
    REQUIRE( !config.GetOpt( "test_section", "missing_uint_key", uintOut ) );

    REQUIRE( intOut == 0 );
    REQUIRE( uintOut == 0 );
}

TEST_CASE( "Config GetPath without XDG_CONFIG_HOME", "[config][ini]" )
{
    EnvGuard xdgGuard( "XDG_CONFIG_HOME" );
    xdgGuard.Unset();

    auto home = getenv( "HOME" );
    REQUIRE( home != nullptr );

    auto path = Config::GetPath( "test.ini" );
    REQUIRE( path == std::string( home ) + "/.config/ModernCore/test.ini" );
}