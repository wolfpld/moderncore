#include <catch2/catch_all.hpp>

#include <poll.h>
#include <thread>

#include <src/wayland/TimerRegistry.hpp>
#include <src/wayland/WaylandTimer.hpp>
#include <src/util/Clock.hpp>

namespace
{
    bool TimerFdReadable( int fd, int timeoutMs )
    {
        pollfd p { .fd = fd, .events = POLLIN };
        return poll( &p, 1, timeoutMs ) > 0 && ( p.revents & POLLIN );
    }

    void SleepMs( int ms )
    {
        poll( nullptr, 0, ms );
    }
}

TEST_CASE( "Registry owns a valid disarmed timerfd", "[timer][timerfd]" )
{
    auto reg = std::make_shared<TimerRegistry>();

    REQUIRE( reg->PollFd() >= 0 );
    REQUIRE( !TimerFdReadable( reg->PollFd(), 10 ) );
    REQUIRE( reg->TimeoutMs( 0 ) == -1 );
    REQUIRE( !reg->Earliest().has_value() );
}

TEST_CASE( "TimeoutMs reports remaining time with minMs floor", "[timer][timerfd]" )
{
    auto reg = std::make_shared<TimerRegistry>();
    const auto timer = reg->Add( 10000, 0, [] {} );

    REQUIRE( reg->TimeoutMs( 0 ) > 9000 );
    REQUIRE( reg->TimeoutMs( 0 ) <= 10000 );
    REQUIRE( reg->TimeoutMs( 5000 ) > 8500 );

    const auto due = reg->Add( 0, 0, [] {} );
    REQUIRE( reg->TimeoutMs( 0 ) == 0 );
    REQUIRE( reg->TimeoutMs( 1 ) == 1 );
    REQUIRE( reg->TimeoutMs( 5000 ) == 5000 );
}

TEST_CASE( "Armed timerfd becomes readable at its deadline and FireDue clears it", "[timer][timerfd]" )
{
    auto reg = std::make_shared<TimerRegistry>();
    const auto timer = reg->Add( 100, 0, [] {} );

    REQUIRE( !TimerFdReadable( reg->PollFd(), 20 ) );
    REQUIRE( TimerFdReadable( reg->PollFd(), 2000 ) );
    REQUIRE( TimerFdReadable( reg->PollFd(), 0 ) );

    reg->FireDue( GetTimeMicro(), [] { return true; } );
    REQUIRE( !TimerFdReadable( reg->PollFd(), 20 ) );
}

TEST_CASE( "Zero-delay timer is due immediately", "[timer][timerfd]" )
{
    auto reg = std::make_shared<TimerRegistry>();
    const auto timer = reg->Add( 0, 0, [] {} );

    REQUIRE( reg->Earliest().value() <= GetTimeMicro() );
    REQUIRE( reg->TimeoutMs( 0 ) == 0 );
    REQUIRE( TimerFdReadable( reg->PollFd(), 100 ) );
}

TEST_CASE( "FireDue runs expired timers and leaves future ones", "[timer][timerfd]" )
{
    auto reg = std::make_shared<TimerRegistry>();
    int fired = 0, future = 0;
    const auto oneShot = reg->Add( 0, 0, [&] { ++fired; } );
    const auto far = reg->Add( 60000, 0, [&] { ++future; } );

    reg->FireDue( GetTimeMicro(), [] { return true; } );
    REQUIRE( fired == 1 );
    REQUIRE( future == 0 );

    REQUIRE( !oneShot->Active() );
    REQUIRE( far->Active() );

    reg->FireDue( GetTimeMicro(), [] { return true; } );
    REQUIRE( fired == 1 );

    reg->FireDue( GetTimeMicro() + 60000000, [] { return true; } );
    REQUIRE( future == 1 );
    REQUIRE( reg->TimeoutMs( 0 ) == -1 );
}

TEST_CASE( "Repeating timer re-arms after FireDue", "[timer][timerfd]" )
{
    auto reg = std::make_shared<TimerRegistry>();
    int fired = 0;
    const auto timer = reg->Add( 0, 3600, [&] { ++fired; } );

    reg->FireDue( GetTimeMicro(), [] { return true; } );
    REQUIRE( fired == 1 );
    REQUIRE( timer->Active() );

    const uint64_t now = GetTimeMicro();
    REQUIRE( *reg->Earliest() >= now + 3500000 );
    REQUIRE( *reg->Earliest() <= now + 3600000 + 50000 );
    REQUIRE( !TimerFdReadable( reg->PollFd(), 10 ) );
}

TEST_CASE( "FireDue with a false keepRunning predicate stops callbacks before delivery", "[timer][timerfd]" )
{
    auto reg = std::make_shared<TimerRegistry>();
    int fired = 0;
    const auto timer = reg->Add( 0, 0, [&] { ++fired; } );

    reg->FireDue( GetTimeMicro(), [] { return false; } );
    REQUIRE( fired == 0 );
    REQUIRE( !timer->Active() );
}

TEST_CASE( "Timerfd error revents are impossible while open; disarming leaves fd pollable", "[timer][timerfd]" )
{
    auto reg = std::make_shared<TimerRegistry>();
    {
        const auto timer = reg->Add( 0, 50, [] {} );
        reg->FireDue( GetTimeMicro(), [] { return true; } );
    }
    pollfd p { .fd = reg->PollFd(), .events = POLLIN };
    REQUIRE( poll( &p, 1, 0 ) == 0 );
}

TEST_CASE( "WaylandTimer destructor cancels the pending timer", "[timer][handle]" )
{
    auto reg = std::make_shared<TimerRegistry>();
    int fired = 0;
    {
        const auto timer = reg->Add( 0, 50, [&] { ++fired; } );
        REQUIRE( timer->Active() );
    }

    REQUIRE( !reg->Earliest().has_value() );
    REQUIRE( reg->TimeoutMs( 0 ) == -1 );
    REQUIRE( !TimerFdReadable( reg->PollFd(), 20 ) );
    REQUIRE( fired == 0 );
}

TEST_CASE( "Stop disarms and is idempotent", "[timer][handle]" )
{
    auto reg = std::make_shared<TimerRegistry>();
    const auto timer = reg->Add( 0, 50, [] {} );

    timer->Stop();
    REQUIRE( !timer->Active() );
    timer->Stop();
    REQUIRE( !reg->Earliest().has_value() );
}

TEST_CASE( "Reset re-arms from now", "[timer][handle]" )
{
    auto reg = std::make_shared<TimerRegistry>();
    int fired = 0;
    const auto timer = reg->Add( 10000, 0, [&] { ++fired; } );

    REQUIRE( *reg->Earliest() > GetTimeMicro() + 9000000 );
    timer->Reset( 0 );
    REQUIRE( *reg->Earliest() <= GetTimeMicro() );

    reg->FireDue( GetTimeMicro(), [] { return true; } );
    REQUIRE( fired == 1 );
}

TEST_CASE( "Reset revives a fired one-shot handle", "[timer][handle]" )
{
    auto reg = std::make_shared<TimerRegistry>();
    int fired = 0;
    const auto timer = reg->Add( 0, 0, [&] { ++fired; } );

    reg->FireDue( GetTimeMicro(), [] { return true; } );
    REQUIRE( fired == 1 );
    REQUIRE( !timer->Active() );

    timer->Reset( 0 );
    REQUIRE( timer->Active() );
    reg->FireDue( GetTimeMicro(), [] { return true; } );
    REQUIRE( fired == 2 );
    REQUIRE( !timer->Active() );
}

TEST_CASE( "Callback cancelling another handle skips its firing", "[timer][handle]" )
{
    auto reg = std::make_shared<TimerRegistry>();
    int other = 0;
    std::unique_ptr<WaylandTimer> victim;
    const auto killer = reg->Add( 0, 0, [&] { victim->Stop(); } );
    victim = reg->Add( 0, 0, [&] { ++other; } );

    reg->FireDue( GetTimeMicro(), [] { return true; } );
    REQUIRE( other == 0 );
    REQUIRE( !victim->Active() );
}

TEST_CASE( "FireDue swallows callback exceptions and the queue stays usable", "[timer][handle]" )
{
    auto reg = std::make_shared<TimerRegistry>();
    const auto bomb = reg->Add( 0, 0, [] { throw std::runtime_error( "boom" ); } );

    REQUIRE_NOTHROW( reg->FireDue( GetTimeMicro(), [] { return true; } ) );
    REQUIRE( !bomb->Active() );

    int fired = 0;
    const auto next = reg->Add( 0, 0, [&] { ++fired; } );
    reg->FireDue( GetTimeMicro(), [] { return true; } );
    REQUIRE( fired == 1 );
}

TEST_CASE( "Shutdown closes the fd and makes all operations inert", "[timer][shutdown]" )
{
    auto reg = std::make_shared<TimerRegistry>();
    int fired = 0;
    const auto timer = reg->Add( 0, 50, [&] { ++fired; } );

    reg->Shutdown();
    REQUIRE( reg->PollFd() == -1 );
    REQUIRE( !timer->Active() );

    timer->Reset( 0 );
    timer->Stop();
    REQUIRE( !timer->Active() );

    const auto inert = reg->Add( 0, 0, [&] { ++fired; } );
    REQUIRE( !inert->Active() );
    inert->Reset( 0 );
    inert->Stop();

    reg->FireDue( GetTimeMicro(), [] { return true; } );
    REQUIRE( fired == 0 );

    REQUIRE_NOTHROW( reg->Shutdown() );
}

TEST_CASE( "Handles keep the registry alive after the owner drops it", "[timer][shutdown]" )
{
    std::shared_ptr<TimerRegistry> reg = std::make_shared<TimerRegistry>();
    int fired = 0;
    const auto timer = reg->Add( 0, 50, [&] { ++fired; } );
    const int fd = reg->PollFd();

    reg.reset();
    REQUIRE( timer->Active() );
    REQUIRE( TimerFdReadable( fd, 100 ) );

    timer->Stop();
    REQUIRE( !timer->Active() );
}

TEST_CASE( "Cross-thread registration arms the timerfd and wakes a blocked poll", "[timer][threads]" )
{
    auto reg = std::make_shared<TimerRegistry>();
    int fired = 0;
    std::unique_ptr<WaylandTimer> timer;

    std::thread worker( [&]
    {
        SleepMs( 50 );
        timer = reg->Add( 100, 0, [&] { ++fired; } );
    } );

    const uint64_t start = GetTimeMicro();
    REQUIRE( TimerFdReadable( reg->PollFd(), 3000 ) );
    const uint64_t elapsedMs = ( GetTimeMicro() - start ) / 1000;
    worker.join();

    REQUIRE( elapsedMs >= 40 );
    REQUIRE( elapsedMs < 2900 );

    reg->FireDue( GetTimeMicro(), [] { return true; } );
    REQUIRE( fired == 1 );
}

TEST_CASE( "Timers registered before firing run in deadline order in one FireDue", "[timer][timerfd]" )
{
    auto reg = std::make_shared<TimerRegistry>();
    std::vector<int> order;
    const auto a = reg->Add( 0, 0, [&] { order.push_back( 1 ); } );
    const auto b = reg->Add( 0, 0, [&] { order.push_back( 2 ); } );
    const auto c = reg->Add( 30, 0, [&] { order.push_back( 3 ); } );

    SleepMs( 40 );
    reg->FireDue( GetTimeMicro(), [] { return true; } );
    REQUIRE( order == std::vector<int>( { 1, 2, 3 } ) );
    REQUIRE( reg->TimeoutMs( 0 ) == -1 );
}
