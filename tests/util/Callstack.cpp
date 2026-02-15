#include "TestUtils.hpp"
#include <catch2/catch_all.hpp>
#include <src/util/Callstack.hpp>
#include <stdio.h>

TEST_CASE( "GetCallstack captures frames", "[callstack][capture]" )
{
    SECTION( "Captures current callstack" )
    {
        GetCallstack( data );
        REQUIRE( data.count > 0 );
        REQUIRE( data.count <= 64 );
    }

    SECTION( "CallstackData contains valid addresses" )
    {
        GetCallstack( data );
        for( int i = 0; i < data.count; i++ )
        {
            REQUIRE( data.addr[i] != nullptr );
        }
    }
}

static void callstackDepth1( CallstackData& out )
{
    GetCallstack( data );
    out = data;
}

static void callstackDepth2( CallstackData& out )
{
    callstackDepth1( out );
}

static void callstackDepth3( CallstackData& out )
{
    callstackDepth2( out );
}

TEST_CASE( "Callstack depth varies with nesting", "[callstack][depth]" )
{
    CallstackData data1, data2, data3;

    callstackDepth1( data1 );
    callstackDepth2( data2 );
    callstackDepth3( data3 );

    REQUIRE( data3.count >= data2.count );
    REQUIRE( data2.count >= data1.count );
}

TEST_CASE( "PrintCallstack produces valid output", "[callstack][print]" )
{
    GetCallstack( data );

    OutputCapture capture;
    PrintCallstack( data );
    std::string output = stripAnsi( capture.getOutput() );

    SECTION( "Contains Callstack header" )
    {
        REQUIRE( output.find( "Callstack:" ) != std::string::npos );
    }

    SECTION( "Contains numbered frames or ellipsis" )
    {
        bool hasNumberedFrame = output.find( ". " ) != std::string::npos;
        bool hasEllipsis = output.find( "…" ) != std::string::npos;
        REQUIRE( ( hasNumberedFrame || hasEllipsis ) );
    }

    SECTION( "Frame format is valid" )
    {
        bool hasUnknown = output.find( "<unknown>" ) != std::string::npos;
        bool hasBracket = output.find( "[" ) != std::string::npos;
        bool hasHexAddr = output.find( "0x" ) != std::string::npos;
        REQUIRE( ( hasUnknown || hasBracket || hasHexAddr ) );
    }
}

static std::string capturePrintCallstack( const CallstackData& data, int skip )
{
    OutputCapture capture;
    PrintCallstack( data, skip );
    return stripAnsi( capture.getOutput() );
}

static void callstackNested4( CallstackData& out )
{
    GetCallstack( data );
    out = data;
}
static void callstackNested3( CallstackData& out ) { callstackNested4( out ); }
static void callstackNested2( CallstackData& out ) { callstackNested3( out ); }
static void callstackNested1( CallstackData& out ) { callstackNested2( out ); }

TEST_CASE( "Skip parameter works correctly", "[callstack][skip]" )
{
    CallstackData data;
    callstackNested1( data );

    std::string out0 = capturePrintCallstack( data, 0 );
    std::string out1 = capturePrintCallstack( data, 1 );
    std::string out2 = capturePrintCallstack( data, 2 );

    auto frames0 = extractFrames( out0 );
    auto frames1 = extractFrames( out1 );
    auto frames2 = extractFrames( out2 );

    if( frames0.size() < 3 || frames1.size() < 2 || frames2.size() < 1 )
    {
        SKIP( "Not enough frames captured for skip test" );
    }

    SECTION( "Skip reduces frame count" )
    {
        REQUIRE( frames0.size() > frames1.size() );
        REQUIRE( frames1.size() > frames2.size() );
        REQUIRE( frames0.size() - frames1.size() == 1 );
        REQUIRE( frames0.size() - frames2.size() == 2 );
    }

    SECTION( "Bottom frames are preserved" )
    {
        for( size_t i = 0; i < frames1.size(); i++ )
        {
            REQUIRE( frames1[i] == frames0[i + 1] );
        }
        for( size_t i = 0; i < frames2.size(); i++ )
        {
            REQUIRE( frames2[i] == frames0[i + 2] );
        }
    }
}

TEST_CASE( "Callstack edge cases", "[callstack][edge]" )
{
    SECTION( "Empty callstack handled" )
    {
        CallstackData empty{};
        empty.count = 0;
        REQUIRE_NOTHROW( PrintCallstack( empty ) );
    }

    SECTION( "Skip exceeds depth" )
    {
        GetCallstack( data );
        std::string output = capturePrintCallstack( data, 100 );
        REQUIRE( countFrames( output ) == 0 );
    }

    SECTION( "Skip equals count" )
    {
        GetCallstack( data );
        REQUIRE_NOTHROW( PrintCallstack( data, data.count ) );
    }
}