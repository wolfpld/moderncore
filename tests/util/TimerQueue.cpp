#include <catch2/catch_all.hpp>

#include <algorithm>
#include <atomic>
#include <memory>
#include <random>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include <src/util/TimerQueue.hpp>

namespace
{
    uint64_t Add( TimerQueue& queue, uint64_t deadlineUs, uint64_t intervalUs, TimerCallback cb )
    {
        return queue.Add( deadlineUs, intervalUs, std::make_shared<TimerCallback>( std::move( cb ) ) );
    }

    bool Empty( const TimerQueue& queue )
    {
        return !queue.Earliest().has_value();
    }
}

TEST_CASE( "One-shot ordering and no-early-fire", "[timer]" )
{
    TimerQueue q;
    std::vector<const char*> order;

    Add( q, 300, 0, [&] { order.push_back( "a" ); } );
    Add( q, 100, 0, [&] { order.push_back( "b" ); } );
    Add( q, 250, 0, [&] { order.push_back( "c" ); } );

    REQUIRE( q.Earliest() == std::optional<uint64_t>( 100 ) );

    REQUIRE( q.Fire( 99 ) == 0 );
    REQUIRE( order.empty() );

    REQUIRE( q.Fire( 100 ) == 1 );
    REQUIRE( order == std::vector<const char*>( { "b" } ) );

    REQUIRE( q.Fire( 100 ) == 0 );
    REQUIRE( q.Fire( 400 ) == 2 );
    REQUIRE( order == std::vector<const char*>( { "b", "c", "a" } ) );
    REQUIRE( Empty( q ) );
}

TEST_CASE( "Equal deadlines fire in registration order", "[timer]" )
{
    TimerQueue q;
    std::vector<int> order;

    for( int i = 0; i < 5; ++i )
        Add( q, 100, 0, [&, i] { order.push_back( i ); } );

    REQUIRE( q.Fire( 100 ) == 5 );
    REQUIRE( order == std::vector<int>( { 0, 1, 2, 3, 4 } ) );
}

TEST_CASE( "Repeating timers are fixed-delay from the fire time", "[timer]" )
{
    TimerQueue q;
    int fired = 0;

    Add( q, 100, 50, [&] { ++fired; } );

    REQUIRE( q.Fire( 100 ) == 1 );
    REQUIRE( fired == 1 );
    REQUIRE( q.Earliest() == std::optional<uint64_t>( 150 ) );

    REQUIRE( q.Fire( 1000 ) == 1 );
    REQUIRE( fired == 2 );
    REQUIRE( q.Earliest() == std::optional<uint64_t>( 1050 ) );

    REQUIRE( q.Fire( 1100 ) == 1 );
    REQUIRE( fired == 3 );
    REQUIRE( q.Earliest() == std::optional<uint64_t>( 1150 ) );

    REQUIRE( q.Fire( 1140 ) == 0 );
    REQUIRE( q.Earliest() == std::optional<uint64_t>( 1150 ) );
}

TEST_CASE( "Removing during the firing batch skips the firing", "[timer]" )
{
    TimerQueue q;
    int aFired = 0, bFired = 0;
    const uint64_t b = Add( q, 200, 0, [&] { ++bFired; } );
    Add( q, 100, 0, [&] { ++aFired; (void)q.Remove( b ); } );

    REQUIRE( q.Fire( 300 ) == 1 );
    REQUIRE( aFired == 1 );
    REQUIRE( bFired == 0 );
    REQUIRE( Empty( q ) );
}

TEST_CASE( "Self-removal from the callback disarms a repeating timer", "[timer]" )
{
    TimerQueue q;
    int fired = 0;
    uint64_t id = 0;
    id = Add( q, 100, 50, [&] { ++fired; if( fired == 2 ) (void)q.Remove( id ); } );

    REQUIRE( q.Fire( 100 ) == 1 );
    REQUIRE( q.Fire( 200 ) == 1 );
    REQUIRE( fired == 2 );
    REQUIRE( Empty( q ) );
}

TEST_CASE( "Requeue re-arms an armed timer", "[timer]" )
{
    TimerQueue q;
    int fired = 0;
    const uint64_t id = Add( q, 100, 0, [&] { ++fired; } );

    REQUIRE( q.Requeue( id, 500 ) );
    REQUIRE( q.Earliest() == std::optional<uint64_t>( 500 ) );

    REQUIRE( q.Fire( 200 ) == 0 );
    REQUIRE( q.Fire( 600 ) == 1 );
    REQUIRE( fired == 1 );

    REQUIRE( !q.Requeue( id, 700 ) );
    REQUIRE( Empty( q ) );
}

TEST_CASE( "Requeue from a repeating timer callback replaces the auto-rearm", "[timer]" )
{
    TimerQueue q;
    int fired = 0;
    uint64_t id = 0;
    id = Add( q, 100, 50, [&]
    {
        ++fired;
        if( fired == 1 ) REQUIRE( q.Requeue( id, 1000 ) );
    } );

    REQUIRE( q.Fire( 100 ) == 1 );
    REQUIRE( q.Earliest() == std::optional<uint64_t>( 1000 ) );
    REQUIRE( q.Fire( 500 ) == 0 );
    REQUIRE( q.Fire( 1000 ) == 1 );
    REQUIRE( q.Earliest() == std::optional<uint64_t>( 1050 ) );
}

TEST_CASE( "Add during Fire runs on the next batch", "[timer]" )
{
    TimerQueue q;
    int late = 0;
    Add( q, 100, 0, [&] { Add( q, 50, 0, [&] { ++late; } ); } );

    REQUIRE( q.Fire( 100 ) == 1 );
    REQUIRE( late == 0 );
    REQUIRE( q.Fire( 100 ) == 1 );
    REQUIRE( late == 1 );
    REQUIRE( Empty( q ) );
}

TEST_CASE( "Remove of unknown or already-fired ids fails", "[timer]" )
{
    TimerQueue q;
    const uint64_t id = Add( q, 100, 0, [] {} );

    REQUIRE( q.Remove( id ) );
    REQUIRE( !q.Remove( id ) );
    REQUIRE( !q.Remove( 12345 ) );
    REQUIRE( Empty( q ) );
}

TEST_CASE( "keepRunning predicate discards the rest of the batch", "[timer]" )
{
    TimerQueue q;
    int aFired = 0, bFired = 0;
    Add( q, 100, 0, [&] { ++aFired; } );
    Add( q, 200, 0, [&] { ++bFired; } );

    const auto keepRunning = [&] { return aFired == 0; };
    REQUIRE( q.Fire( 300, keepRunning ) == 1 );
    REQUIRE( aFired == 1 );
    REQUIRE( bFired == 0 );
    REQUIRE( Empty( q ) );
    REQUIRE( q.Fire( 300, keepRunning ) == 0 );
}

TEST_CASE( "Clear drops pending timers including re-arms", "[timer]" )
{
    TimerQueue q;
    int fired = 0;
    Add( q, 100, 10, [&] { ++fired; q.Clear(); } );
    Add( q, 500, 0, [&] { ++fired; } );

    REQUIRE( q.Fire( 100 ) == 1 );
    REQUIRE( fired == 1 );
    REQUIRE( Empty( q ) );
    REQUIRE( q.Fire( 1000 ) == 0 );
}

TEST_CASE( "Earliest deadline tracks removals and re-adds", "[timer]" )
{
    TimerQueue q;
    REQUIRE( !q.Earliest().has_value() );

    const uint64_t a = Add( q, 100, 0, [] {} );
    const uint64_t b = Add( q, 200, 0, [] {} );
    REQUIRE( q.Earliest() == std::optional<uint64_t>( 100 ) );

    REQUIRE( q.Remove( a ) );
    REQUIRE( q.Earliest() == std::optional<uint64_t>( 200 ) );
    REQUIRE( q.Contains( b ) );
    REQUIRE( !q.Contains( a ) );
}

TEST_CASE( "Contains reports armed timers only", "[timer]" )
{
    TimerQueue q;
    int fired = 0;
    const uint64_t id = Add( q, 100, 0, [&] { ++fired; } );

    REQUIRE( q.Contains( id ) );
    REQUIRE( q.Fire( 100 ) == 1 );
    REQUIRE( !q.Contains( id ) );
    REQUIRE( !q.Contains( 999 ) );
}

TEST_CASE( "Large shuffled batches fire in deadline order", "[timer]" )
{
    TimerQueue q;
    std::mt19937 rng( 42 );
    std::vector<uint64_t> deadlines;
    for( uint64_t d = 10; d < 10000; d += 37 )
    {
        deadlines.push_back( d );
        Add( q, d, 0, [d, &deadlines] { deadlines.push_back( d ); } );
    }
    std::shuffle( deadlines.begin(), deadlines.end(), rng );
    const size_t adds = deadlines.size();
    REQUIRE( q.Fire( UINT64_MAX ) == adds );
    const std::vector<uint64_t> fired( deadlines.begin() + adds, deadlines.end() );
    REQUIRE( std::is_sorted( fired.begin(), fired.end() ) );
    REQUIRE( Empty( q ) );
}

TEST_CASE( "Concurrent Add/Remove is race-free", "[timer]" )
{
    TimerQueue q;
    std::atomic<int> firedCount { 0 };
    auto cb = [&] { ++firedCount; };

    std::vector<std::thread> threads;
    for( int t = 0; t < 4; ++t )
    {
        threads.emplace_back( [&]
        {
            for( int i = 0; i < 500; ++i )
            {
                const uint64_t id = Add( q, static_cast<uint64_t>( i ) % 1000, 0, cb );
                if( i % 3 == 0 ) (void)q.Remove( id );
                else (void)q.Requeue( id, static_cast<uint64_t>( i ) % 1000 );
            }
        } );
    }

    uint64_t firedTotal = 0;
    for( uint64_t now = 0; now <= 1000; now += 50 ) firedTotal += q.Fire( now );
    for( auto& th: threads ) th.join();
    firedTotal += q.Fire( UINT64_MAX );

    REQUIRE( Empty( q ) );
    REQUIRE( firedTotal == static_cast<uint64_t>( firedCount.load() ) );
    REQUIRE( firedTotal > 0 );
}

TEST_CASE( "Fire on an empty queue is a no-op", "[timer]" )
{
    TimerQueue q;
    REQUIRE( q.Fire( 1000 ) == 0 );
    REQUIRE( Empty( q ) );
    REQUIRE( !q.Remove( 1 ) );
    REQUIRE( !q.Requeue( 1, 100 ) );
}

TEST_CASE( "Fire twice at the same now fires each timer once", "[timer]" )
{
    TimerQueue q;
    int oneShot = 0, repeat = 0;
    Add( q, 100, 0, [&] { ++oneShot; } );
    Add( q, 100, 50, [&] { ++repeat; } );

    REQUIRE( q.Fire( 150 ) == 2 );
    REQUIRE( oneShot == 1 );
    REQUIRE( repeat == 1 );
    REQUIRE( q.Earliest() == std::optional<uint64_t>( 200 ) );
    REQUIRE( q.Fire( 150 ) == 0 );
    REQUIRE( repeat == 1 );
}

TEST_CASE( "Requeue from a one-shot callback re-arms it", "[timer]" )
{
    TimerQueue q;
    int fired = 0;
    uint64_t id = 0;
    id = Add( q, 100, 0, [&]
    {
        ++fired;
        if( fired == 1 ) REQUIRE( q.Requeue( id, 300 ) );
    } );

    REQUIRE( q.Fire( 100 ) == 1 );
    REQUIRE( q.Earliest() == std::optional<uint64_t>( 300 ) );
    REQUIRE( q.Contains( id ) );
    REQUIRE( q.Fire( 350 ) == 1 );
    REQUIRE( fired == 2 );
    REQUIRE( Empty( q ) );
}

TEST_CASE( "Second Requeue from a callback overrides the first", "[timer]" )
{
    TimerQueue q;
    int fired = 0;
    uint64_t id = 0;
    id = Add( q, 100, 50, [&]
    {
        ++fired;
        if( fired == 1 )
        {
            (void)q.Requeue( id, 400 );
            (void)q.Requeue( id, 700 );
        }
    } );

    REQUIRE( q.Fire( 100 ) == 1 );
    REQUIRE( q.Earliest() == std::optional<uint64_t>( 700 ) );
}

TEST_CASE( "Clear from a callback discards the rest of the batch", "[timer]" )
{
    TimerQueue q;
    int aFired = 0, bFired = 0;
    Add( q, 100, 0, [&] { ++aFired; q.Clear(); } );
    Add( q, 150, 50, [&] { ++bFired; } );

    REQUIRE( q.Fire( 200 ) == 1 );
    REQUIRE( aFired == 1 );
    REQUIRE( bFired == 0 );
    REQUIRE( Empty( q ) );
}

TEST_CASE( "Predicate false before the first callback discards everything", "[timer]" )
{
    TimerQueue q;
    int fired = 0;
    Add( q, 100, 0, [&] { ++fired; } );
    Add( q, 200, 50, [&] { ++fired; } );

    const auto never = [] { return false; };
    REQUIRE( q.Fire( 300, never ) == 0 );
    REQUIRE( fired == 0 );
    REQUIRE( Empty( q ) );
}

TEST_CASE( "Removing a repeating entry before its firing skips it without re-arm", "[timer]" )
{
    TimerQueue q;
    int bFired = 0;
    const uint64_t b = Add( q, 150, 50, [&] { ++bFired; } );
    Add( q, 100, 0, [&] { (void)q.Remove( b ); } );

    REQUIRE( q.Fire( 200 ) == 1 );
    REQUIRE( bFired == 0 );
    REQUIRE( !q.Contains( b ) );
    REQUIRE( Empty( q ) );
}

TEST_CASE( "Ids are never reused", "[timer]" )
{
    TimerQueue q;
    const uint64_t a = Add( q, 100, 0, [] {} );
    REQUIRE( q.Remove( a ) );
    const uint64_t b = Add( q, 100, 0, [] {} );
    REQUIRE( b != a );
}

TEST_CASE( "Mixed one-shot and repeating batch keeps deadline order", "[timer]" )
{
    TimerQueue q;
    std::vector<const char*> order;
    Add( q, 100, 25, [&] { order.push_back( "r1" ); } );
    Add( q, 100, 0, [&] { order.push_back( "o1" ); } );
    Add( q, 50, 0, [&] { order.push_back( "o0" ); } );

    REQUIRE( q.Fire( 100 ) == 3 );
    REQUIRE( order == std::vector<const char*>( { "o0", "r1", "o1" } ) );
    REQUIRE( q.Earliest() == std::optional<uint64_t>( 125 ) );
}

TEST_CASE( "WithEarliest observes the earliest deadline", "[timer]" )
{
    TimerQueue q;
    std::optional<uint64_t> seen1 = 1;
    q.WithEarliest( [&]( const std::optional<uint64_t>& earliest ) { seen1 = earliest; } );
    REQUIRE( !seen1.has_value() );

    Add( q, 100, 0, [] {} );
    Add( q, 50, 0, [] {} );
    std::optional<uint64_t> seen2 = 0;
    q.WithEarliest( [&]( const std::optional<uint64_t>& earliest ) { seen2 = earliest; } );
    REQUIRE( seen2 == std::optional<uint64_t>( 50 ) );
}

TEST_CASE( "Throwing callback does not corrupt the queue", "[timer]" )
{
    TimerQueue q;
    int bFired = 0;
    Add( q, 100, 0, [] { throw std::runtime_error( "boom" ); } );
    const uint64_t b = Add( q, 150, 50, [&] { ++bFired; } );

    REQUIRE_THROWS_AS( q.Fire( 200 ), std::runtime_error );
    REQUIRE( bFired == 0 );
    REQUIRE( !q.Contains( b ) );
    REQUIRE( Empty( q ) );

    Add( q, 100, 0, [&] { ++bFired; } );
    REQUIRE( q.Fire( 200 ) == 1 );
    REQUIRE( bFired == 1 );
}

TEST_CASE( "A repeating timer whose callback throws is dropped", "[timer]" )
{
    TimerQueue q;
    int fired = 0;
    Add( q, 100, 50, [&] { ++fired; if( fired == 1 ) throw 1; } );

    REQUIRE_THROWS( q.Fire( 100 ) );
    REQUIRE( Empty( q ) );
    REQUIRE( q.Fire( 200 ) == 0 );
    REQUIRE( fired == 1 );
}

TEST_CASE( "Requeue from another callback skips the pending fire", "[timer]" )
{
    TimerQueue q;
    int bFired = 0;
    const uint64_t b = Add( q, 150, 0, [&] { ++bFired; } );
    Add( q, 100, 0, [&] { (void)q.Requeue( b, 900 ); } );

    REQUIRE( q.Fire( 200 ) == 1 );
    REQUIRE( bFired == 0 );
    REQUIRE( q.Earliest() == std::optional<uint64_t>( 900 ) );
    REQUIRE( q.Fire( 200 ) == 0 );
    REQUIRE( q.Fire( 950 ) == 1 );
    REQUIRE( bFired == 1 );
    REQUIRE( Empty( q ) );
}

TEST_CASE( "Requeue preserves registration order among equal deadlines", "[timer]" )
{
    TimerQueue q;
    std::vector<char> order;
    const uint64_t a = Add( q, 300, 0, [&] { order.push_back( 'a' ); } );
    Add( q, 200, 0, [&] { order.push_back( 'b' ); } );
    Add( q, 200, 0, [&] { order.push_back( 'c' ); } );

    REQUIRE( q.Requeue( a, 200 ) );
    REQUIRE( q.Fire( 200 ) == 3 );
    REQUIRE( order == std::vector<char>( { 'a', 'b', 'c' } ) );
}
