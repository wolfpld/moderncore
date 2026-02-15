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

    SECTION( "Lowercase hex digits" )
    {
        std::string url = "%2f%2F";
        UrlDecode( url );
        REQUIRE( url == "//" );

        std::string url2 = "%aa%AA%aA%Aa";
        UrlDecode( url2 );
        REQUIRE( url2.size() == 4 );
    }

    SECTION( "Incomplete hex - single digit" )
    {
        std::string url = "test%2";
        UrlDecode( url );
        REQUIRE( url == "test" );
    }

    SECTION( "Invalid hex characters throw" )
    {
        std::string url = "%GG";
        REQUIRE_THROWS_AS( UrlDecode( url ), std::invalid_argument );

        std::string url2 = "%ZZ";
        REQUIRE_THROWS_AS( UrlDecode( url2 ), std::invalid_argument );
    }

    SECTION( "Partial hex is parsed" )
    {
        std::string url = "%2G";
        UrlDecode( url );
        REQUIRE( url.size() == 1 );
        REQUIRE( static_cast<unsigned char>( url[0] ) == 0x02 );
    }

    SECTION( "Percent at string start" )
    {
        std::string url = "%20start";
        UrlDecode( url );
        REQUIRE( url == " start" );
    }

    SECTION( "All printable ASCII via encoding" )
    {
        for( int i = 32; i < 127; i++ )
        {
            char encoded[4];
            snprintf( encoded, sizeof( encoded ), "%%%02X", i );
            std::string url = encoded;
            UrlDecode( url );
            REQUIRE( url.size() == 1 );
            REQUIRE( url[0] == static_cast<char>( i ) );
        }
    }

    SECTION( "High byte values via encoding" )
    {
        std::string url = "%FF";
        UrlDecode( url );
        REQUIRE( url.size() == 1 );
        REQUIRE( static_cast<unsigned char>( url[0] ) == 0xFF );

        std::string url2 = "%80";
        UrlDecode( url2 );
        REQUIRE( url2.size() == 1 );
        REQUIRE( static_cast<unsigned char>( url2[0] ) == 0x80 );
    }

    SECTION( "Long string with many encodings" )
    {
        std::string url;
        for( int i = 0; i < 100; i++ )
        {
            url += "%20";
        }
        UrlDecode( url );
        REQUIRE( url.size() == 100 );
        for( char c : url )
        {
            REQUIRE( c == ' ' );
        }
    }
}

TEST_CASE( "UrlDecode benchmarks", "[!benchmark][url]" )
{
    SECTION( "Decode heavily encoded string" )
    {
        std::string url;
        for( int i = 0; i < 10000; i++ )
        {
            url += "%20";
        }
        BENCHMARK( "Decode 10000 encoded spaces" )
        {
            auto copy = url;
            UrlDecode( copy );
            return copy;
        };
    }

    SECTION( "Decode string with no encoding" )
    {
        std::string url( 10000, 'X' );
        BENCHMARK( "Decode 10000 unencoded chars" )
        {
            auto copy = url;
            UrlDecode( copy );
            return copy;
        };
    }

    SECTION( "Decode mixed content" )
    {
        std::string url;
        for( int i = 0; i < 2500; i++ )
        {
            url += "Hello%20World%21";
        }
        BENCHMARK( "Decode mixed 20000 chars" )
        {
            auto copy = url;
            UrlDecode( copy );
            return copy;
        };
    }
}
