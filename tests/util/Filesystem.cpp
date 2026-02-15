#include <catch2/catch_all.hpp>
#include <src/util/Filesystem.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

TEST_CASE( "CreateDirectories functionality", "[filesystem][directories]" )
{
    char baseTemplate[] = "/tmp/test_mcore_XXXXXX";
    char* basePath = mkdtemp( baseTemplate );
    REQUIRE( basePath != nullptr );
    std::string base( basePath );

    SECTION( "Single subdirectory creation" )
    {
        std::string path = base + "/single";

        bool result = CreateDirectories( path );
        REQUIRE( result == true );

        struct stat buf;
        REQUIRE( stat( path.c_str(), &buf ) == 0 );
        REQUIRE( S_ISDIR( buf.st_mode ) );

        rmdir( path.c_str() );
    }

    SECTION( "Multiple sibling directories in parallel" )
    {
        std::string pathA = base + "/multi/a";
        std::string pathB = base + "/multi/b";
        std::string pathC = base + "/multi/c";

        bool resultA = CreateDirectories( pathA );
        bool resultB = CreateDirectories( pathB );
        bool resultC = CreateDirectories( pathC );

        REQUIRE( resultA == true );
        REQUIRE( resultB == true );
        REQUIRE( resultC == true );

        struct stat buf;
        REQUIRE( stat( pathA.c_str(), &buf ) == 0 );
        REQUIRE( S_ISDIR( buf.st_mode ) );
        REQUIRE( stat( pathB.c_str(), &buf ) == 0 );
        REQUIRE( S_ISDIR( buf.st_mode ) );
        REQUIRE( stat( pathC.c_str(), &buf ) == 0 );
        REQUIRE( S_ISDIR( buf.st_mode ) );

        rmdir( ( base + "/multi/a" ).c_str() );
        rmdir( ( base + "/multi/b" ).c_str() );
        rmdir( ( base + "/multi/c" ).c_str() );
        rmdir( ( base + "/multi" ).c_str() );
    }

    SECTION( "Deep nesting - three levels" )
    {
        std::string path = base + "/deep/nested/path";

        bool result = CreateDirectories( path );
        REQUIRE( result == true );

        struct stat buf;
        REQUIRE( stat( path.c_str(), &buf ) == 0 );
        REQUIRE( S_ISDIR( buf.st_mode ) );
        REQUIRE( stat( ( base + "/deep/nested" ).c_str(), &buf ) == 0 );
        REQUIRE( S_ISDIR( buf.st_mode ) );
        REQUIRE( stat( ( base + "/deep" ).c_str(), &buf ) == 0 );
        REQUIRE( S_ISDIR( buf.st_mode ) );

        rmdir( ( base + "/deep/nested/path" ).c_str() );
        rmdir( ( base + "/deep/nested" ).c_str() );
        rmdir( ( base + "/deep" ).c_str() );
    }

    SECTION( "Existing directory handling" )
    {
        std::string path = base + "/existing";

        bool result1 = CreateDirectories( path );
        REQUIRE( result1 == true );

        bool result2 = CreateDirectories( path );
        REQUIRE( result2 == true );

        struct stat buf;
        REQUIRE( stat( path.c_str(), &buf ) == 0 );

        rmdir( path.c_str() );
    }

    SECTION( "Permission denied - read-only parent directory" )
    {
        std::string parentPath = base + "/readonly";

        int ret = mkdir( parentPath.c_str(), 0555 );
        REQUIRE( ret == 0 );

        std::string childPath = parentPath + "/subdir";
        bool result = CreateDirectories( childPath );
        REQUIRE( result == false );

        chmod( parentPath.c_str(), 0755 );
        rmdir( parentPath.c_str() );
    }

    SECTION( "Empty path" )
    {
        bool result = CreateDirectories( "" );
        REQUIRE( result == false );
    }

    SECTION( "Path with trailing slash" )
    {
        std::string path = base + "/slash/";

        bool result = CreateDirectories( path );
        REQUIRE( result == true );

        struct stat buf;
        REQUIRE( stat( ( base + "/slash" ).c_str(), &buf ) == 0 );
        REQUIRE( S_ISDIR( buf.st_mode ) );

        rmdir( ( base + "/slash" ).c_str() );
    }

    rmdir( base.c_str() );
}
