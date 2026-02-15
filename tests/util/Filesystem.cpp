#include <catch2/catch_all.hpp>
#include <src/util/Filesystem.hpp>
#include "TestUtils.hpp"
#include <stdio.h>
#include <sys/stat.h>

TEST_CASE( "CreateDirectories functionality", "[filesystem][directories]" )
{
    TempDir baseDir = TempDir::create();
    REQUIRE( !baseDir.str().empty() );

    SECTION( "Single subdirectory creation" )
    {
        std::string path = baseDir.filePath( "single" );

        bool result = CreateDirectories( path );
        REQUIRE( result == true );

        struct stat buf;
        REQUIRE( stat( path.c_str(), &buf ) == 0 );
        REQUIRE( S_ISDIR( buf.st_mode ) );
    }

    SECTION( "Multiple sibling directories in parallel" )
    {
        std::string pathA = baseDir.filePath( "multi/a" );
        std::string pathB = baseDir.filePath( "multi/b" );
        std::string pathC = baseDir.filePath( "multi/c" );

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
    }

    SECTION( "Deep nesting - three levels" )
    {
        std::string path = baseDir.filePath( "deep/nested/path" );

        bool result = CreateDirectories( path );
        REQUIRE( result == true );

        struct stat buf;
        REQUIRE( stat( path.c_str(), &buf ) == 0 );
        REQUIRE( S_ISDIR( buf.st_mode ) );
        REQUIRE( stat( baseDir.filePath( "deep/nested" ).c_str(), &buf ) == 0 );
        REQUIRE( S_ISDIR( buf.st_mode ) );
        REQUIRE( stat( baseDir.filePath( "deep" ).c_str(), &buf ) == 0 );
        REQUIRE( S_ISDIR( buf.st_mode ) );
    }

    SECTION( "Existing directory handling" )
    {
        std::string path = baseDir.filePath( "existing" );

        bool result1 = CreateDirectories( path );
        REQUIRE( result1 == true );

        bool result2 = CreateDirectories( path );
        REQUIRE( result2 == true );

        struct stat buf;
        REQUIRE( stat( path.c_str(), &buf ) == 0 );
    }

    SECTION( "Permission denied - read-only parent directory" )
    {
        std::string parentPath = baseDir.filePath( "readonly" );

        int ret = mkdir( parentPath.c_str(), 0555 );
        REQUIRE( ret == 0 );

        std::string childPath = parentPath + "/subdir";
        bool result = CreateDirectories( childPath );
        REQUIRE( result == false );

        chmod( parentPath.c_str(), 0755 );
    }

    SECTION( "Empty path" )
    {
        bool result = CreateDirectories( "" );
        REQUIRE( result == false );
    }

    SECTION( "Path with trailing slash" )
    {
        std::string path = baseDir.filePath( "slash/" );

        bool result = CreateDirectories( path );
        REQUIRE( result == true );

        struct stat buf;
        REQUIRE( stat( baseDir.filePath( "slash" ).c_str(), &buf ) == 0 );
        REQUIRE( S_ISDIR( buf.st_mode ) );
    }
}
