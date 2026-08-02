#include "TestUtils.hpp"
#include <catch2/catch_all.hpp>
#include <src/util/Vector2.hpp>

TEST_CASE( "Vector2 construction", "[vector2]" )
{
    SECTION( "Default constructor zeroes both components" )
    {
        Vector2<int> v;
        REQUIRE( v.x == 0 );
        REQUIRE( v.y == 0 );
    }

    SECTION( "Scalar constructor sets both components" )
    {
        Vector2<int> v( 5 );
        REQUIRE( v.x == 5 );
        REQUIRE( v.y == 5 );
    }

    SECTION( "Two-component constructor" )
    {
        Vector2<int> v( 3, 7 );
        REQUIRE( v.x == 3 );
        REQUIRE( v.y == 7 );
    }

    SECTION( "Converting constructor" )
    {
        Vector2<float> vi( 2, 4 );
        Vector2<double> vd( vi );
        REQUIRE( vd.x == 2.0 );
        REQUIRE( vd.y == 4.0 );
    }

    SECTION( "Converting constructor from int to float" )
    {
        Vector2<int> vi( 1, 2 );
        Vector2<float> vf( vi );
        REQUIRE( vf.x == 1.0f );
        REQUIRE( vf.y == 2.0f );
    }
}

TEST_CASE( "Vector2 comparisons", "[vector2]" )
{
    SECTION( "Equality" )
    {
        Vector2<int> a( 1, 2 );
        Vector2<int> b( 1, 2 );
        Vector2<int> c( 2, 1 );

        REQUIRE( a == b );
        REQUIRE( !( a == c ) );
        REQUIRE( a != c );
        REQUIRE( !( a != b ) );
    }

    SECTION( "Float equality" )
    {
        Vector2<float> a( 1.5f, 2.5f );
        Vector2<float> b( 1.5f, 2.5f );
        REQUIRE( a == b );
    }
}

TEST_CASE( "Vector2 compound assignment", "[vector2]" )
{
    SECTION( "Add assign" )
    {
        Vector2<int> a( 1, 2 );
        a += Vector2<int>( 3, 4 );
        REQUIRE( a.x == 4 );
        REQUIRE( a.y == 6 );
    }

    SECTION( "Subtract assign" )
    {
        Vector2<int> a( 5, 7 );
        a -= Vector2<int>( 2, 3 );
        REQUIRE( a.x == 3 );
        REQUIRE( a.y == 4 );
    }

    SECTION( "Multiply assign" )
    {
        Vector2<int> a( 2, 3 );
        a *= Vector2<int>( 4, 5 );
        REQUIRE( a.x == 8 );
        REQUIRE( a.y == 15 );
    }

    SECTION( "Chained operations" )
    {
        Vector2<int> a( 10, 10 );
        a += Vector2<int>( 1, 1 ) -= Vector2<int>( 2, 2 );
        REQUIRE( a.x == 9 );
        REQUIRE( a.y == 9 );
    }
}

TEST_CASE( "Vector2 arithmetic", "[vector2]" )
{
    SECTION( "Addition" )
    {
        const auto r = Vector2<int>( 1, 2 ) + Vector2<int>( 3, 4 );
        REQUIRE( r.x == 4 );
        REQUIRE( r.y == 6 );
    }

    SECTION( "Subtraction" )
    {
        const auto r = Vector2<int>( 5, 7 ) - Vector2<int>( 2, 3 );
        REQUIRE( r.x == 3 );
        REQUIRE( r.y == 4 );
    }

    SECTION( "Scalar multiplication" )
    {
        const auto r = Vector2<int>( 2, 3 ) * 4;
        REQUIRE( r.x == 8 );
        REQUIRE( r.y == 12 );
    }

    SECTION( "Scalar division" )
    {
        const auto r = Vector2<int>( 8, 12 ) / 4;
        REQUIRE( r.x == 2 );
        REQUIRE( r.y == 3 );
    }

    SECTION( "Scalar multiplication with double" )
    {
        const auto r = Vector2<float>( 2.5f, 3.5f ) * 2.0;
        REQUIRE( r.x == 5.0f );
        REQUIRE( r.y == 7.0f );
    }

    SECTION( "Scalar division with double" )
    {
        const auto r = Vector2<float>( 5.0f, 7.0f ) / 2.0;
        REQUIRE( r.x == 2.5f );
        REQUIRE( r.y == 3.5f );
    }
}

TEST_CASE( "Vector2 ordering", "[vector2]" )
{
    SECTION( "Orders by x then y" )
    {
        REQUIRE( Vector2<int>( 1, 2 ) < Vector2<int>( 2, 1 ) );
        REQUIRE( Vector2<int>( 1, 2 ) < Vector2<int>( 1, 3 ) );
        REQUIRE( !( Vector2<int>( 1, 3 ) < Vector2<int>( 1, 2 ) ) );
        REQUIRE( !( Vector2<int>( 2, 1 ) < Vector2<int>( 1, 2 ) ) );
    }

    SECTION( "Equal vectors are not less" )
    {
        REQUIRE( !( Vector2<int>( 4, 4 ) < Vector2<int>( 4, 4 ) ) );
    }
}
