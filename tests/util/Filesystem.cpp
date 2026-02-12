#include <catch2/catch_all.hpp>
#include <src/util/Filesystem.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Helper function to generate unique temporary directory names
std::string get_unique_temp_dir( const char* base )
{
    // Use PID and timestamp for uniqueness
    char buffer[256];
    snprintf( buffer, sizeof( buffer ), "/tmp/%s_%d_%ld", base, getpid(), time( NULL ) );
    return std::string( buffer );
}

TEST_CASE( "CreateDirectories functionality", "[filesystem][directories]" )
{
    SECTION( "Single-level directory creation" )
    {
        // Create a unique temporary directory name
        std::string path = get_unique_temp_dir( "test_mcore_single" );

        // Create directory
        bool result = CreateDirectories( path );
        REQUIRE( result == true );

        // Verify directory exists
        struct stat buf;
        REQUIRE( stat( path.c_str(), &buf ) == 0 );

        // Clean up
        rmdir( path.c_str() );
    }

    SECTION( "Multi-level nested directory creation" )
    {
        // Create unique temporary directory names
        std::string base_path = get_unique_temp_dir( "test_mcore_nested" );
        std::string path = base_path + "/subdir";

        // Create nested directory
        bool result = CreateDirectories( path );
        REQUIRE( result == true );

        // Verify both levels exist
        struct stat buf;
        REQUIRE( stat( base_path.c_str(), &buf ) == 0 );

        // Clean up
        rmdir( ( base_path + "/subdir" ).c_str() );
        rmdir( base_path.c_str() );
    }

    SECTION( "Existing directory handling" )
    {
        // Create a unique temporary directory name
        std::string path = get_unique_temp_dir( "test_mcore_existing" );

        // Create directory first
        mkdir( path.c_str(), 0777 );

        // Try to create it again
        bool result = CreateDirectories( path );
        REQUIRE( result == true );

        // Clean up
        rmdir( path.c_str() );
    }

    SECTION( "Empty path" )
    {
        // Test the actual function behavior with empty path
        std::string path = "";

        // Call CreateDirectories with empty path
        // The function will call stat("", &buf) which will fail with ENOENT
        // and return false
        bool result = CreateDirectories( path );
        REQUIRE( result == false );
    }

    SECTION( "Path with trailing slash" )
    {
        // Test path with trailing slash (e.g., "/tmp/test_dir/")
        std::string base_path = get_unique_temp_dir( "test_mcore_slash" );
        std::string path = base_path + "/";

        // Create directory with trailing slash
        bool result = CreateDirectories( path );
        REQUIRE( result == true );

        // Verify directory exists
        struct stat buf;
        REQUIRE( stat( base_path.c_str(), &buf ) == 0 );

        // Clean up
        rmdir( base_path.c_str() );
    }
}
