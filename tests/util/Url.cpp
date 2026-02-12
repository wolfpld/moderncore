#include <catch2/catch_all.hpp>
#include <src/util/Url.hpp>

TEST_CASE( "UrlDecode functionality", "[url][decode]" )
{
    SECTION( "Basic URL decoding" )
    {
        std::string url1 = "Hello%20World";
        UrlDecode( url1 );
        REQUIRE( url1 == "Hello World" );

        std::string url2 = "Test%2Fwith%20slash";
        UrlDecode( url2 );
        REQUIRE( url2 == "Test/with slash" );
    }

    SECTION( "Multiple percent-encoded sequences" )
    {
        std::string url = "%41%42%43%44%45%46";
        UrlDecode( url );
        REQUIRE( url == "ABCDEF" );
    }

    SECTION( "Single character codes" )
    {
        std::string url = "%20";
        UrlDecode( url );
        REQUIRE( url == " " );

        std::string url2 = "%41";
        UrlDecode( url2 );
        REQUIRE( url2 == "A" );
    }

    SECTION( "Empty string" )
    {
        std::string url = "";
        UrlDecode( url );
        REQUIRE( url.empty() );
    }

    SECTION( "No percent-encoded sequences" )
    {
        std::string url = "Hello World";
        UrlDecode( url );
        REQUIRE( url == "Hello World" );
    }

    SECTION( "Trailing incomplete sequence" )
    {
        std::string url = "abc%";
        UrlDecode( url );
        REQUIRE( url == "abc" );
    }

    SECTION( "Overlapping replacements" )
    {
        std::string url = "%41%41%42";
        UrlDecode( url );
        REQUIRE( url == "AAB" );
    }
}
