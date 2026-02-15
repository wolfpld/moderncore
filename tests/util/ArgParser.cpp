#include <catch2/catch_all.hpp>
#include <src/util/ArgParser.hpp>
#include <string.h>

TEST_CASE( "ParseBoolean functionality", "[argparser][boolean]" )
{
    SECTION( "Valid boolean strings - true values" )
    {
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

    SECTION( "Invalid strings - should throw ArgParseException" )
    {
        REQUIRE_THROWS_AS( ParseBoolean( "invalid" ), ArgParseException );
        REQUIRE_THROWS_AS( ParseBoolean( "maybe" ), ArgParseException );
        REQUIRE_THROWS_AS( ParseBoolean( "" ), ArgParseException );
        REQUIRE_THROWS_AS( ParseBoolean( " " ), ArgParseException );
        REQUIRE_THROWS_AS( ParseBoolean( "  " ), ArgParseException );
    }

    SECTION( "Exception message contains input value" )
    {
        try
        {
            ParseBoolean( "invalid" );
            FAIL( "Expected ArgParseException to be thrown" );
        }
        catch( const ArgParseException& e )
        {
            std::string msg( e.what() );
            REQUIRE( msg.find( "invalid" ) != std::string::npos );
            REQUIRE( msg.find( "is not a valid boolean argument" ) != std::string::npos );
        }
    }

    SECTION( "Case insensitivity - lowercase" )
    {
        REQUIRE( ParseBoolean( "on" ) == true );
        REQUIRE( ParseBoolean( "off" ) == false );
        REQUIRE( ParseBoolean( "true" ) == true );
        REQUIRE( ParseBoolean( "false" ) == false );
    }

    SECTION( "Case insensitivity - uppercase" )
    {
        REQUIRE( ParseBoolean( "ON" ) == true );
        REQUIRE( ParseBoolean( "OFF" ) == false );
        REQUIRE( ParseBoolean( "TRUE" ) == true );
        REQUIRE( ParseBoolean( "FALSE" ) == false );
        REQUIRE( ParseBoolean( "YES" ) == true );
        REQUIRE( ParseBoolean( "NO" ) == false );
    }

    SECTION( "Case insensitivity - mixed case" )
    {
        REQUIRE( ParseBoolean( "On" ) == true );
        REQUIRE( ParseBoolean( "TrUe" ) == true );
        REQUIRE( ParseBoolean( "FaLsE" ) == false );
        REQUIRE( ParseBoolean( "EnableD" ) == true );
        REQUIRE( ParseBoolean( "Disabled" ) == false );
        REQUIRE( ParseBoolean( "AcTiVe" ) == true );
        REQUIRE( ParseBoolean( "inACTIVE" ) == false );
    }

    SECTION( "Single character values" )
    {
        REQUIRE( ParseBoolean( "1" ) == true );
        REQUIRE( ParseBoolean( "0" ) == false );
        REQUIRE( ParseBoolean( "t" ) == true );
        REQUIRE( ParseBoolean( "T" ) == true );
        REQUIRE( ParseBoolean( "f" ) == false );
        REQUIRE( ParseBoolean( "F" ) == false );
        REQUIRE( ParseBoolean( "y" ) == true );
        REQUIRE( ParseBoolean( "Y" ) == true );
        REQUIRE( ParseBoolean( "n" ) == false );
        REQUIRE( ParseBoolean( "N" ) == false );
    }

    SECTION( "Strings with numbers should throw" )
    {
        REQUIRE_THROWS_AS( ParseBoolean( "2" ), ArgParseException );
        REQUIRE_THROWS_AS( ParseBoolean( "10" ), ArgParseException );
        REQUIRE_THROWS_AS( ParseBoolean( "01" ), ArgParseException );
        REQUIRE_THROWS_AS( ParseBoolean( "true1" ), ArgParseException );
        REQUIRE_THROWS_AS( ParseBoolean( "1true" ), ArgParseException );
    }

    SECTION( "Strings with special characters should throw" )
    {
        REQUIRE_THROWS_AS( ParseBoolean( "on!" ), ArgParseException );
        REQUIRE_THROWS_AS( ParseBoolean( "@yes" ), ArgParseException );
        REQUIRE_THROWS_AS( ParseBoolean( "tr ue" ), ArgParseException );
        REQUIRE_THROWS_AS( ParseBoolean( "on\n" ), ArgParseException );
        REQUIRE_THROWS_AS( ParseBoolean( "\toff" ), ArgParseException );
    }

    SECTION( "Long strings should throw" )
    {
        std::string longString( 1000, 'a' );
        REQUIRE_THROWS_AS( ParseBoolean( longString.c_str() ), ArgParseException );
    }

    SECTION( "Similar but invalid strings" )
    {
        REQUIRE_THROWS_AS( ParseBoolean( "tru" ), ArgParseException );
        REQUIRE_THROWS_AS( ParseBoolean( "truue" ), ArgParseException );
        REQUIRE_THROWS_AS( ParseBoolean( "enablee" ), ArgParseException );
        REQUIRE_THROWS_AS( ParseBoolean( "enabl" ), ArgParseException );
        REQUIRE_THROWS_AS( ParseBoolean( "activ" ), ArgParseException );
        REQUIRE_THROWS_AS( ParseBoolean( "activee" ), ArgParseException );
        REQUIRE_THROWS_AS( ParseBoolean( "o" ), ArgParseException );
        REQUIRE_THROWS_AS( ParseBoolean( "of" ), ArgParseException );
    }
}
