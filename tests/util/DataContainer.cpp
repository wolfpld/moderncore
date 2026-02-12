#include <catch2/catch_all.hpp>
#include <src/util/DataBuffer.hpp>
#include <src/util/DataContainer.hpp>

TEST_CASE( "DataContainer concept validation", "[datacontainer][concept]" )
{
    SECTION( "DataBuffer satisfies the concept" )
    {
        const char data[] = "test";
        DataBuffer buffer( data, sizeof( data ) - 1 );

        REQUIRE( DataContainer<decltype( buffer )> );
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
}
