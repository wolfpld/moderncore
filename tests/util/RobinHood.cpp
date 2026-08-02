#include "TestUtils.hpp"
#include <catch2/catch_all.hpp>
#include <src/util/RobinHood.hpp>
#include <string>

TEST_CASE( "RobinHood flat map", "[robinhood][map]" )
{
    SECTION( "Insert and find" )
    {
        unordered_flat_map<int, std::string> map;
        map.emplace( 1, "one" );
        map.emplace( 2, "two" );

        REQUIRE( map.size() == 2 );
        REQUIRE( map.find( 1 ) != map.end() );
        REQUIRE( map.find( 1 )->second == "one" );
        REQUIRE( map.find( 2 )->second == "two" );
        REQUIRE( map.find( 3 ) == map.end() );
    }

    SECTION( "Operator[] inserts defaults" )
    {
        unordered_flat_map<int, std::string> map;
        map[1] = "first";
        REQUIRE( map[1] == "first" );
        REQUIRE( map.size() == 1 );
    }

    SECTION( "Erase removes entries" )
    {
        unordered_flat_map<int, std::string> map;
        map.emplace( 1, "one" );
        map.emplace( 2, "two" );

        REQUIRE( map.erase( 1 ) == 1 );
        REQUIRE( map.size() == 1 );
        REQUIRE( map.find( 1 ) == map.end() );
        REQUIRE( map.erase( 42 ) == 0 );
    }

    SECTION( "Iteration visits all entries" )
    {
        unordered_flat_map<int, std::string> map;
        for( int i = 0; i < 100; i++ )
        {
            map.emplace( i, std::to_string( i ) );
        }

        int count = 0;
        for( const auto& [key, value] : map )
        {
            REQUIRE( value == std::to_string( key ) );
            count++;
        }
        REQUIRE( count == 100 );
    }

    SECTION( "Count and contains" )
    {
        unordered_flat_map<int, std::string> map;
        map.emplace( 7, "seven" );
        REQUIRE( map.count( 7 ) == 1 );
        REQUIRE( map.count( 8 ) == 0 );
    }

    SECTION( "Const access" )
    {
        unordered_flat_map<int, std::string> map;
        map.emplace( 1, "one" );

        const auto& ref = map;
        REQUIRE( ref.find( 1 ) != ref.end() );
        REQUIRE( ref.find( 1 )->second == "one" );
    }

    SECTION( "String keys" )
    {
        unordered_flat_map<std::string, int> map;
        map.emplace( "alpha", 1 );
        map.emplace( "beta", 2 );

        REQUIRE( map.at( "alpha" ) == 1 );
        REQUIRE( map.at( "beta" ) == 2 );
    }
}

TEST_CASE( "RobinHood flat set", "[robinhood][set]" )
{
    SECTION( "Insert and find" )
    {
        unordered_flat_set<std::string> set;
        set.insert( "apple" );
        set.insert( "banana" );

        REQUIRE( set.size() == 2 );
        REQUIRE( set.find( "apple" ) != set.end() );
        REQUIRE( set.find( "cherry" ) == set.end() );
    }

    SECTION( "Duplicate insertion is ignored" )
    {
        unordered_flat_set<int> set;
        set.insert( 1 );
        set.insert( 1 );
        REQUIRE( set.size() == 1 );
    }

    SECTION( "Erase" )
    {
        unordered_flat_set<int> set;
        set.insert( 1 );
        set.insert( 2 );

        REQUIRE( set.erase( 1 ) == 1 );
        REQUIRE( set.size() == 1 );
        REQUIRE( set.find( 1 ) == set.end() );
    }

    SECTION( "Iteration" )
    {
        unordered_flat_set<int> set;
        for( int i = 0; i < 50; i++ )
        {
            set.insert( i );
        }

        int count = 0;
        for( const auto& value : set )
        {
            REQUIRE( value >= 0 );
            REQUIRE( value < 50 );
            count++;
        }
        REQUIRE( count == 50 );
    }

    SECTION( "Rehash and growth" )
    {
        unordered_flat_set<int> set;
        for( int i = 0; i < 1000; i++ )
        {
            set.insert( i );
        }
        REQUIRE( set.size() == 1000 );
        for( int i = 0; i < 1000; i++ )
        {
            REQUIRE( set.find( i ) != set.end() );
        }
    }
}
