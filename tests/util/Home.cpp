#include "TestUtils.hpp"
#include <catch2/catch_all.hpp>
#include <src/util/Home.hpp>
#include <stdlib.h>
#include <unistd.h>

TEST_CASE( "Home functionality", "[home][path]" )
{
    SECTION( "GetHome with HOME environment variable set" )
    {
        EnvGuard homeGuard( "HOME" );
        homeGuard.Set( "/tmp/test_home_dir" );

        std::string homePath = GetHome();

        REQUIRE( homePath == "/tmp/test_home_dir" );
    }

    SECTION( "GetHome with HOME environment variable unset" )
    {
        EnvGuard homeGuard( "HOME" );
        homeGuard.Unset();

        std::string homePath = GetHome();

        REQUIRE( !homePath.empty() );
        REQUIRE( homePath.length() > 0 );
    }

    SECTION( "ExpandHome with tilde prefix" )
    {
        EnvGuard homeGuard( "HOME" );
        homeGuard.Set( "/tmp/test_home_dir" );

        std::string path = ExpandHome( "~/test/file.txt" );

        REQUIRE( path == "/tmp/test_home_dir/test/file.txt" );
    }

    SECTION( "ExpandHome without tilde prefix" )
    {
        EnvGuard homeGuard( "HOME" );
        homeGuard.Set( "/tmp/test_home_dir" );

        const char* testPath = "/absolute/path.txt";
        std::string path = ExpandHome( testPath );

        REQUIRE( path == testPath );
    }

    SECTION( "ExpandHome with tilde and empty suffix" )
    {
        EnvGuard homeGuard( "HOME" );
        homeGuard.Set( "/tmp/test_home_dir" );

        std::string path = ExpandHome( "~" );

        REQUIRE( path == "/tmp/test_home_dir" );
    }
}
