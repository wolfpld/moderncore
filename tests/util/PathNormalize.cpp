#include <catch2/catch_all.hpp>
#include <src/util/PathNormalize.hpp>
#include <string>

TEST_CASE( "NormalizePath functionality", "[callstack][normalize]" )
{
    SECTION( "Absolute paths are preserved" )
    {
        REQUIRE( NormalizePath( "/usr/bin/foo.c" ) == "/usr/bin/foo.c" );
        REQUIRE( NormalizePath( "/" ) == "/" );
    }

    SECTION( "Dot components are removed" )
    {
        REQUIRE( NormalizePath( "/foo/./bar" ) == "/foo/bar" );
        REQUIRE( NormalizePath( "/foo/." ) == "/foo" );
    }

    SECTION( "Dot-dot pops the previous component" )
    {
        REQUIRE( NormalizePath( "/foo/bar/.." ) == "/foo" );
        REQUIRE( NormalizePath( "/foo/../bar" ) == "/bar" );
        REQUIRE( NormalizePath( "/foo/.." ) == "/" );
    }

    SECTION( "Dot-dot at the root is clamped" )
    {
        REQUIRE( NormalizePath( "/.." ) == "/" );
        REQUIRE( NormalizePath( "/../x" ) == "/x" );
        REQUIRE( NormalizePath( "/foo/../.." ) == "/" );
        REQUIRE( NormalizePath( "/../../.." ) == "/" );
    }

    SECTION( "Relative paths are rejected" )
    {
        REQUIRE( NormalizePath( "foo/bar" ).empty() );
        REQUIRE( NormalizePath( "foo" ).empty() );
    }

    SECTION( "Repeated slashes are collapsed" )
    {
        REQUIRE( NormalizePath( "/foo//bar" ) == "/foo/bar" );
        REQUIRE( NormalizePath( "//" ) == "/" );
    }

    SECTION( "Empty string is rejected" )
    {
        REQUIRE( NormalizePath( "" ).empty() );
    }
}
