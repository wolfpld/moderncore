#include <catch2/catch_all.hpp>
#include <src/util/Home.hpp>
#include <stdlib.h>
#include <unistd.h>

TEST_CASE( "Home functionality", "[home][path]" )
{
    SECTION( "GetHome with HOME environment variable set" )
    {
        // Save original HOME value
        const char* originalHome = getenv( "HOME" );

        // Set HOME environment variable
        const char* testHome = "/tmp/test_home_dir";
        setenv( "HOME", testHome, 1 );

        // Call GetHome
        std::string homePath = GetHome();

        // Verify it returns the HOME environment variable
        REQUIRE( homePath == testHome );

        // Restore original HOME value
        if( originalHome )
        {
            setenv( "HOME", originalHome, 1 );
        }
        else
        {
            unsetenv( "HOME" );
        }
    }

    SECTION( "GetHome with HOME environment variable unset" )
    {
        // Save original HOME value
        const char* originalHome = getenv( "HOME" );

        // Unset HOME environment variable
        if( originalHome )
        {
            unsetenv( "HOME" );
        }

        // Call GetHome - should use getpwuid_r
        std::string homePath = GetHome();

        // Verify it returns a non-empty string (uses getpwuid_r)
        REQUIRE( !homePath.empty() );
        REQUIRE( homePath.length() > 0 );

        // Restore original HOME value
        if( originalHome )
        {
            setenv( "HOME", originalHome, 1 );
        }
    }

    SECTION( "ExpandHome with tilde prefix" )
    {
        // Save original HOME value
        const char* originalHome = getenv( "HOME" );

        // Set HOME environment variable
        const char* testHome = "/tmp/test_home_dir";
        setenv( "HOME", testHome, 1 );

        // Test ExpandHome with tilde prefix
        std::string path = ExpandHome( "~/test/file.txt" );

        // Verify it replaces tilde with home directory
        REQUIRE( path == "/tmp/test_home_dir/test/file.txt" );

        // Restore original HOME value
        if( originalHome )
        {
            setenv( "HOME", originalHome, 1 );
        }
        else
        {
            unsetenv( "HOME" );
        }
    }

    SECTION( "ExpandHome without tilde prefix" )
    {
        // Save original HOME value
        const char* originalHome = getenv( "HOME" );

        // Set HOME environment variable (should not affect result)
        const char* testHome = "/tmp/test_home_dir";
        setenv( "HOME", testHome, 1 );

        // Test ExpandHome without tilde prefix
        const char* testPath = "/absolute/path.txt";
        std::string path = ExpandHome( testPath );

        // Verify it returns the path unchanged
        REQUIRE( path == testPath );

        // Restore original HOME value
        if( originalHome )
        {
            setenv( "HOME", originalHome, 1 );
        }
        else
        {
            unsetenv( "HOME" );
        }
    }

    SECTION( "ExpandHome with tilde and empty suffix" )
    {
        // Save original HOME value
        const char* originalHome = getenv( "HOME" );

        // Set HOME environment variable
        const char* testHome = "/tmp/test_home_dir";
        setenv( "HOME", testHome, 1 );

        // Test ExpandHome with just tilde
        std::string path = ExpandHome( "~" );

        // Verify it returns just the home directory
        REQUIRE( path == testHome );

        // Restore original HOME value
        if( originalHome )
        {
            setenv( "HOME", originalHome, 1 );
        }
        else
        {
            unsetenv( "HOME" );
        }
    }
}
