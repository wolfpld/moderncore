#include "TestUtils.hpp"
#include <catch2/catch_all.hpp>
#include <src/util/Listener.hpp>
#include <functional>
#include <vector>

TEST_CASE( "Listener receives emitted signals", "[listener]" )
{
    wl_signal signal;
    wl_signal_init( &signal );

    SECTION( "Single listener receives the emitted data" )
    {
        int received = 0;
        Listener<int> listener( signal, [&]( int* data ) { received = *data; } );

        int value = 42;
        wl_signal_emit( &signal, &value );
        REQUIRE( received == 42 );
    }

    SECTION( "Listeners are notified in registration order" )
    {
        std::vector<int> order;
        Listener<int> first( signal, [&]( int* ) { order.push_back( 1 ); } );
        Listener<int> second( signal, [&]( int* ) { order.push_back( 2 ); } );

        int value = 0;
        wl_signal_emit( &signal, &value );

        REQUIRE( order.size() == 2 );
        REQUIRE( order[0] == 1 );
        REQUIRE( order[1] == 2 );
    }

    SECTION( "Emit with no listeners is a no-op" )
    {
        int value = 7;
        wl_signal_emit( &signal, &value ); // must not crash
    }

    SECTION( "Listener destructor removes it from the signal" )
    {
        int called = 0;
        {
            Listener<int> listener( signal, [&]( int* ) { called++; } );
            int value = 1;
            wl_signal_emit( &signal, &value );
            REQUIRE( called == 1 );
        }

        // After destruction the listener must no longer be notified
        int value = 2;
        wl_signal_emit( &signal, &value );
        REQUIRE( called == 1 );
    }

    SECTION( "Listener can be destroyed before the signal" )
    {
        Listener<int>* listener = nullptr;
        {
            // Scoped so the listener is destroyed first
            auto tmp = new Listener<int>( signal, []( int* ) {} );
            listener = tmp;
        }
        delete listener;

        int value = 3;
        wl_signal_emit( &signal, &value ); // must not crash
    }
}

TEST_CASE( "Listener with custom data types", "[listener]" )
{
    wl_signal signal;
    wl_signal_init( &signal );

    SECTION( "Works with struct data" )
    {
        struct Payload
        {
            int a;
            int b;
        };

        Payload received = { 0, 0 };
        Listener<Payload> listener( signal, [&]( Payload* data ) { received = *data; } );

        Payload payload = { 5, 6 };
        wl_signal_emit( &signal, &payload );

        REQUIRE( received.a == 5 );
        REQUIRE( received.b == 6 );
    }

    SECTION( "Works with pointer data" )
    {
        const char* received = nullptr;
        Listener<const char> listener( signal, [&]( const char* data ) { received = data; } );

        const char* text = "hello";
        wl_signal_emit( &signal, const_cast<char*>( text ) );

        REQUIRE( received == text );
    }
}

TEST_CASE( "Listener copyability", "[listener]" )
{
    SECTION( "Is non-copyable" )
    {
        static_assert( !std::is_copy_constructible<Listener<int>>::value );
        static_assert( !std::is_copy_assignable<Listener<int>>::value );
    }
}
