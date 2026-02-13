#include <catch2/catch_all.hpp>
#include <signal.h>
#include <src/util/ArgParser.hpp>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

TEST_CASE( "ParseBoolean functionality", "[argparser][boolean]" )
{
    SECTION( "Valid boolean strings - true values" )
    {
        // Test all true values
        REQUIRE( ParseBoolean( "on" ) == true );
        REQUIRE( ParseBoolean( "true" ) == true );
        REQUIRE( ParseBoolean( "t" ) == true );
        REQUIRE( ParseBoolean( "1" ) == true );
        REQUIRE( ParseBoolean( "yes" ) == true );
        REQUIRE( ParseBoolean( "y" ) == true );
        REQUIRE( ParseBoolean( "enable" ) == true );
        REQUIRE( ParseBoolean( "enabled" ) == true );
        REQUIRE( ParseBoolean( "active" ) == true );
    }

    SECTION( "Valid boolean strings - false values" )
    {
        // Test all false values
        REQUIRE( ParseBoolean( "off" ) == false );
        REQUIRE( ParseBoolean( "false" ) == false );
        REQUIRE( ParseBoolean( "f" ) == false );
        REQUIRE( ParseBoolean( "0" ) == false );
        REQUIRE( ParseBoolean( "no" ) == false );
        REQUIRE( ParseBoolean( "n" ) == false );
        REQUIRE( ParseBoolean( "disable" ) == false );
        REQUIRE( ParseBoolean( "disabled" ) == false );
        REQUIRE( ParseBoolean( "inactive" ) == false );
    }

    SECTION( "Invalid strings - should throw exception" )
    {
        // Test that invalid input throws ArgParseException
        REQUIRE_THROWS( ParseBoolean( "invalid" ) );

        // Test with specific message
        try
        {
            ParseBoolean( "invalid" );
        }
        catch( const ArgParseException& e )
        {
            std::string msg( e.what() );
            REQUIRE( msg.find( "invalid" ) != std::string::npos );
            REQUIRE( msg.find( "is not a valid boolean argument" ) != std::string::npos );
        }
    }

    SECTION( "Case sensitivity" )
    {
        // ParseBoolean is case-insensitive (uses strcasecmp)
        // These should all work regardless of case

        // Lowercase
        REQUIRE( ParseBoolean( "on" ) == true );
        REQUIRE( ParseBoolean( "true" ) == true );

        // Uppercase
        REQUIRE( ParseBoolean( "ON" ) == true );
        REQUIRE( ParseBoolean( "TRUE" ) == true );

        // Mixed case
        REQUIRE( ParseBoolean( "On" ) == true );
        REQUIRE( ParseBoolean( "TrUe" ) == true );
    }
}
