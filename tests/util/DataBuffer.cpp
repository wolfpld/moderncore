#include <catch2/catch_all.hpp>
#include <src/util/DataBuffer.hpp>

TEST_CASE( "DataBuffer functionality", "[databuffer][buffer]" )
{
    SECTION( "Constructor with valid data" )
    {
        const char testData[] = "test data";
        size_t testSize = sizeof( testData ) - 1;
        DataBuffer buffer( testData, testSize );

        REQUIRE( buffer.data() == testData );
        REQUIRE( buffer.size() == testSize );
    }

    SECTION( "Constructor with different data" )
    {
        const char testData[] = "different data";
        size_t testSize = sizeof( testData ) - 1;
        DataBuffer buffer( testData, testSize );

        REQUIRE( buffer.data() == testData );
        REQUIRE( buffer.size() == testSize );
    }
}
