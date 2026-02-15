#include <catch2/catch_all.hpp>
#include <src/util/Clock.hpp>
#include <unistd.h>

TEST_CASE( "GetTimeMicro functionality", "[clock][time]" )
{
    SECTION( "Returns positive values" )
    {
        auto time1 = GetTimeMicro();
        REQUIRE( time1 > 0 );

        auto time2 = GetTimeMicro();
        REQUIRE( time2 > 0 );
    }

    SECTION( "Time is in reasonable range for steady_clock" )
    {
        auto time = GetTimeMicro();

        uint64_t minExpected = 1ULL;
        uint64_t maxExpected = 1000000000000000ULL;

        REQUIRE( time >= minExpected );
        REQUIRE( time <= maxExpected );
    }

    SECTION( "Time increases monotonically across multiple calls" )
    {
        auto time1 = GetTimeMicro();
        auto time2 = GetTimeMicro();
        auto time3 = GetTimeMicro();
        auto time4 = GetTimeMicro();
        auto time5 = GetTimeMicro();

        REQUIRE( time2 >= time1 );
        REQUIRE( time3 >= time2 );
        REQUIRE( time4 >= time3 );
        REQUIRE( time5 >= time4 );
    }

    SECTION( "Time shows measurable progression with sleep" )
    {
        auto time1 = GetTimeMicro();
        usleep( 1000 );
        auto time2 = GetTimeMicro();

        REQUIRE( time2 > time1 );

        uint64_t elapsed = time2 - time1;
        REQUIRE( elapsed >= 500 );
        REQUIRE( elapsed <= 10000 );
    }

    SECTION( "Time shows progression with short sleep" )
    {
        auto time1 = GetTimeMicro();
        usleep( 100 );
        auto time2 = GetTimeMicro();

        REQUIRE( time2 > time1 );
        REQUIRE( ( time2 - time1 ) >= 50 );
    }

    SECTION( "Time shows progression with longer sleep" )
    {
        auto time1 = GetTimeMicro();
        usleep( 10000 );
        auto time2 = GetTimeMicro();

        REQUIRE( time2 > time1 );
        REQUIRE( ( time2 - time1 ) >= 5000 );
        REQUIRE( ( time2 - time1 ) <= 50000 );
    }

    SECTION( "Consecutive calls return different values or same (high precision)" )
    {
        auto time1 = GetTimeMicro();
        auto time2 = GetTimeMicro();

        REQUIRE( time2 >= time1 );
    }

    SECTION( "Time values are consistent in microsecond precision" )
    {
        auto time1 = GetTimeMicro();
        usleep( 500 );
        auto time2 = GetTimeMicro();
        usleep( 500 );
        auto time3 = GetTimeMicro();

        uint64_t elapsed1 = time2 - time1;
        uint64_t elapsed2 = time3 - time2;

        REQUIRE( elapsed1 >= 200 );
        REQUIRE( elapsed2 >= 200 );

        REQUIRE( elapsed1 <= 5000 );
        REQUIRE( elapsed2 <= 5000 );
    }
}
