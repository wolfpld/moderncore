#include <catch2/catch_all.hpp>
#include <src/util/ArgParser.hpp>
#include <string.h>

TEST_CASE( "ParseBoolean valid true values", "[argparser][boolean]" )
{
    auto input = GENERATE( "on", "true", "t", "1", "yes", "y", "enable", "enabled", "active" );
    REQUIRE( ParseBoolean( input ) == true );
}

TEST_CASE( "ParseBoolean valid false values", "[argparser][boolean]" )
{
    auto input = GENERATE( "off", "false", "f", "0", "no", "n", "disable", "disabled", "inactive" );
    REQUIRE( ParseBoolean( input ) == false );
}

TEST_CASE( "ParseBoolean invalid strings throw", "[argparser][boolean]" )
{
    auto input = GENERATE( "invalid", "maybe", "", " ", "  ", "2", "10", "01", "true1", "1true",
                           "on!", "@yes", "tr ue", "on\n", "\toff", "tru", "truue", "enablee",
                           "enabl", "activ", "activee", "o", "of" );
    REQUIRE_THROWS_AS( ParseBoolean( input ), ArgParseException );
}

TEST_CASE( "ParseBoolean case insensitivity - uppercase", "[argparser][boolean]" )
{
    auto [input, expected] = GENERATE( table<const char*, bool>( {
        { "ON", true },
        { "OFF", false },
        { "TRUE", true },
        { "FALSE", false },
        { "YES", true },
        { "NO", false },
    } ) );
    REQUIRE( ParseBoolean( input ) == expected );
}

TEST_CASE( "ParseBoolean case insensitivity - mixed case", "[argparser][boolean]" )
{
    auto [input, expected] = GENERATE( table<const char*, bool>( {
        { "On", true },
        { "TrUe", true },
        { "FaLsE", false },
        { "EnableD", true },
        { "Disabled", false },
        { "AcTiVe", true },
        { "inACTIVE", false },
    } ) );
    REQUIRE( ParseBoolean( input ) == expected );
}

TEST_CASE( "ParseBoolean single character values", "[argparser][boolean]" )
{
    auto [input, expected] = GENERATE( table<const char*, bool>( {
        { "1", true },
        { "0", false },
        { "t", true },
        { "T", true },
        { "f", false },
        { "F", false },
        { "y", true },
        { "Y", true },
        { "n", false },
        { "N", false },
    } ) );
    REQUIRE( ParseBoolean( input ) == expected );
}

TEST_CASE( "ParseBoolean long string throws", "[argparser][boolean]" )
{
    std::string longString( 1000, 'a' );
    REQUIRE_THROWS_AS( ParseBoolean( longString.c_str() ), ArgParseException );
}

TEST_CASE( "ParseBoolean exception message contains input", "[argparser][boolean]" )
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