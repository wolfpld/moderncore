#include <catch2/catch_all.hpp>
#include <src/util/Clock.hpp>
#include <unistd.h>

TEST_CASE( "GetTimeMicro functionality", "[clock][time]" )
{
    SECTION( "Returns non-zero values" )
    {
        auto time1 = GetTimeMicro();
        REQUIRE( time1 > 0 );
    }

    SECTION( "Time increases monotonically" )
    {
        auto time1 = GetTimeMicro();
        auto time2 = GetTimeMicro();
        REQUIRE( time2 >= time1 );
    }

    SECTION( "Multiple calls show progression" )
    {
        auto time1 = GetTimeMicro();
        usleep( 10 );
        auto time2 = GetTimeMicro();
        REQUIRE( time2 > time1 );
    }
}
