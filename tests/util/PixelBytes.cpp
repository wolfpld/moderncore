#include "TestUtils.hpp"
#include <catch2/catch_all.hpp>
#include <src/util/PixelBytes.hpp>
#include <stdint.h>

TEST_CASE( "PixelBytes helpers", "[pixelbytes]" )
{
    SECTION( "PixelCount is width times height" )
    {
        REQUIRE( PixelCount( 2, 3 ) == 6 );
        REQUIRE( PixelCount( 16, 16 ) == 256 );
        REQUIRE( PixelCount( 0, 0 ) == 0 );
    }

    SECTION( "PixelChannelCount uses four channels by default" )
    {
        REQUIRE( PixelChannelCount( 2, 3 ) == 24 );
        REQUIRE( PixelChannelCount( 16, 16 ) == 1024 );
    }

    SECTION( "PixelChannelCount supports custom channel counts" )
    {
        REQUIRE( PixelChannelCount( 2, 3, 1 ) == 6 );
        REQUIRE( PixelChannelCount( 2, 3, 3 ) == 18 );
        REQUIRE( PixelChannelCount( 2, 3, 8 ) == 48 );
    }

    SECTION( "PixelAlloc allocates the expected number of elements" )
    {
        auto* data = PixelAlloc<float>( 4, 5 );
        REQUIRE( data != nullptr );
        // Writing across the full allocation must not crash
        const auto count = PixelChannelCount( 4, 5 );
        for( size_t i = 0; i < count; i++ )
        {
            data[i] = float( i );
        }
        REQUIRE( data[0] == 0.0f );
        REQUIRE( data[count - 1] == float( count - 1 ) );
        delete[] data;
    }

    SECTION( "PixelAlloc with custom channels" )
    {
        auto* data = PixelAlloc<uint8_t>( 2, 2, 1 );
        REQUIRE( data != nullptr );
        delete[] data;
    }

    SECTION( "PixelAlloc of zero size" )
    {
        auto* data = PixelAlloc<int>( 0, 0 );
        REQUIRE( data != nullptr ); // new[] of size 0 returns a valid pointer
        delete[] data;
    }
}
