#include <catch2/catch_all.hpp>
#include <src/util/DataBuffer.hpp>
#include <src/util/MemoryBuffer.hpp>
#include <string>
#include <vector>

TEST_CASE( "MemoryBuffer functionality", "[memorybuffer][buffer]" )
{
    SECTION( "Default constructor" )
    {
        MemoryBuffer memBuffer;

        // Default constructor should have empty buffer
        REQUIRE( memBuffer.data() == nullptr );
        REQUIRE( memBuffer.size() == 0 );
    }

    SECTION( "Constructor with std::vector<char>&& (move semantics)" )
    {
        std::vector<char> testData = { 'H', 'e', 'l', 'l', 'o' };

        // Move the vector into MemoryBuffer
        MemoryBuffer memBuffer( std::move( testData ) );

        // Verify data was moved successfully
        REQUIRE( memBuffer.data() != nullptr );
        REQUIRE( memBuffer.size() == 5 );

        // Verify content using memcmp
        const char* dataPtr = memBuffer.data();
        const char* expected = "Hello";
        REQUIRE( memcmp( dataPtr, expected, 5 ) == 0 );

        // Original vector should be empty after move
        REQUIRE( testData.empty() );
    }

    SECTION( "Constructor with empty vector" )
    {
        std::vector<char> emptyData;

        MemoryBuffer memBuffer( std::move( emptyData ) );

        REQUIRE( memBuffer.size() == 0 );
        // When vector is empty, data() returns nullptr
        REQUIRE( memBuffer.data() == nullptr );
    }

    SECTION( "AsString method returns correct string" )
    {
        std::vector<char> testData = { 'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '!' };

        MemoryBuffer memBuffer( std::move( testData ) );

        // Get string representation
        std::string result = memBuffer.AsString();

        // Verify string content
        const char* expected = "Hello World!";
        REQUIRE( result == expected );
        REQUIRE( result.length() == strlen( expected ) );
    }

    SECTION( "AsString with empty buffer" )
    {
        std::vector<char> emptyData;

        MemoryBuffer memBuffer( std::move( emptyData ) );

        std::string result = memBuffer.AsString();

        REQUIRE( result.empty() );
        REQUIRE( result.length() == 0 );
    }
}
