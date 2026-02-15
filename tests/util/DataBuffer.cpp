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

    SECTION( "Constructor with null data pointer and zero size" )
    {
        DataBuffer buffer( nullptr, 0 );

        REQUIRE( buffer.data() == nullptr );
        REQUIRE( buffer.size() == 0 );
    }

    SECTION( "Constructor with empty string" )
    {
        const char emptyData[] = "";
        DataBuffer buffer( emptyData, 0 );

        REQUIRE( buffer.data() == emptyData );
        REQUIRE( buffer.size() == 0 );
    }

    SECTION( "Constructor with binary data containing null bytes" )
    {
        const char binaryData[] = { 'H', 'e', 'l', 'l', 'o', '\0', 'W', 'o', 'r', 'l', 'd' };
        size_t binarySize = sizeof( binaryData );
        DataBuffer buffer( binaryData, binarySize );

        REQUIRE( buffer.data() == binaryData );
        REQUIRE( buffer.size() == binarySize );
        REQUIRE( buffer.size() == 11 );
    }

    SECTION( "Constructor with large data" )
    {
        std::vector<char> largeData( 1000000, 'X' );
        DataBuffer buffer( largeData.data(), largeData.size() );

        REQUIRE( buffer.data() == largeData.data() );
        REQUIRE( buffer.size() == 1000000 );
    }

    SECTION( "Data is not copied - pointer equality" )
    {
        const char testData[] = "original data";
        DataBuffer buffer( testData, sizeof( testData ) - 1 );

        REQUIRE( buffer.data() == testData );
        REQUIRE( buffer.data() == &testData[0] );
    }

    SECTION( "Single byte data" )
    {
        const char singleByte = 'A';
        DataBuffer buffer( &singleByte, 1 );

        REQUIRE( buffer.data() == &singleByte );
        REQUIRE( buffer.size() == 1 );
        REQUIRE( buffer.data()[0] == 'A' );
    }

    SECTION( "Multiple DataBuffers can point to same data" )
    {
        const char sharedData[] = "shared";
        DataBuffer buffer1( sharedData, sizeof( sharedData ) - 1 );
        DataBuffer buffer2( sharedData, sizeof( sharedData ) - 1 );

        REQUIRE( buffer1.data() == buffer2.data() );
        REQUIRE( buffer1.size() == buffer2.size() );
    }

    SECTION( "Size is independent of data content" )
    {
        const char data[] = "test";

        DataBuffer buffer1( data, 2 );
        DataBuffer buffer2( data, 4 );

        REQUIRE( buffer1.data() == buffer2.data() );
        REQUIRE( buffer1.size() == 2 );
        REQUIRE( buffer2.size() == 4 );
    }
}
