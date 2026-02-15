#include <catch2/catch_all.hpp>
#include <src/util/DataBuffer.hpp>
#include <src/util/DataContainer.hpp>
#include <vector>
#include <string>
#include <array>

TEST_CASE( "DataContainer concept validation", "[datacontainer][concept]" )
{
    SECTION( "DataBuffer satisfies the concept" )
    {
        const char data[] = "test";
        DataBuffer buffer( data, sizeof( data ) - 1 );

        REQUIRE( DataContainer<decltype( buffer )> );
    }

    SECTION( "std::vector<char> satisfies the concept" )
    {
        std::vector<char> vec = { 'a', 'b', 'c' };
        REQUIRE( DataContainer<decltype( vec )> );
    }

    SECTION( "std::vector<uint8_t> satisfies the concept" )
    {
        std::vector<uint8_t> vec = { 0x01, 0x02, 0x03 };
        REQUIRE( DataContainer<decltype( vec )> );
    }

    SECTION( "std::vector<int> satisfies the concept" )
    {
        std::vector<int> vec = { 1, 2, 3 };
        REQUIRE( DataContainer<decltype( vec )> );
    }

    SECTION( "std::string satisfies the concept" )
    {
        std::string str = "hello";
        REQUIRE( DataContainer<decltype( str )> );
    }

    SECTION( "Empty containers satisfy the concept" )
    {
        std::vector<char> emptyVec;
        REQUIRE( DataContainer<decltype( emptyVec )> );

        std::string emptyStr;
        REQUIRE( DataContainer<decltype( emptyStr )> );
    }

    SECTION( "Type mismatch does not satisfy the concept" )
    {
        struct InvalidType
        {
            int value;
        };

        InvalidType invalid;
        REQUIRE( !DataContainer<decltype( invalid )> );
    }

    SECTION( "Type with only size() does not satisfy the concept" )
    {
        struct OnlySize
        {
            size_t size() const { return 0; }
        };

        OnlySize obj;
        REQUIRE( !DataContainer<decltype( obj )> );
    }

    SECTION( "Type with only data() does not satisfy the concept" )
    {
        struct OnlyData
        {
            const void* data() const { return nullptr; }
        };

        OnlyData obj;
        REQUIRE( !DataContainer<decltype( obj )> );
    }

    SECTION( "Type with wrong return type does not satisfy the concept" )
    {
        struct WrongReturnType
        {
            int size() const { return 0; }
            int data() const { return 0; }
        };

        WrongReturnType obj;
        REQUIRE( !DataContainer<decltype( obj )> );
    }

    SECTION( "Type with non-const methods satisfies the concept" )
    {
        struct NonConstOnly
        {
            size_t size() const { return 0; }
            const void* data() const { return nullptr; }
        };

        NonConstOnly obj;
        REQUIRE( DataContainer<decltype( obj )> );
    }

    SECTION( "std::array satisfies the concept" )
    {
        std::array<char, 5> arr = { 'h', 'e', 'l', 'l', 'o' };
        REQUIRE( DataContainer<decltype( arr )> );
    }

    SECTION( "C-style array does not satisfy the concept (no methods)" )
    {
        char arr[] = "test";
        REQUIRE( !DataContainer<decltype( arr )> );
    }
}
