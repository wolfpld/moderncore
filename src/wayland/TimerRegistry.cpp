#include <algorithm>
#include <climits>
#include <errno.h>
#include <string.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include "TimerRegistry.hpp"
#include "WaylandTimer.hpp"
#include "util/Clock.hpp"
#include "util/Logs.hpp"
#include "util/Panic.hpp"

TimerRegistry::TimerRegistry()
{
    m_timerFd = timerfd_create( CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC );
    CheckPanic( m_timerFd != -1, "Failed to create timerfd" );
}

TimerRegistry::~TimerRegistry()
{
    Shutdown();
}

std::unique_ptr<WaylandTimer> TimerRegistry::Add( uint32_t delayMs, uint64_t intervalMs, TimerCallback cb )
{
    const std::scoped_lock lock( m_mutex );
    if( m_shuttingDown )
    {
        mclog( LogLevel::Warning, "Timer registration during shutdown yields an inert handle" );
        return std::unique_ptr<WaylandTimer>( new WaylandTimer( shared_from_this(), 0, 0, nullptr ) );
    }
    const auto cbptr = std::make_shared<TimerCallback>( std::move( cb ) );
    const uint64_t deadline = GetTimeMicro() + uint64_t( delayMs ) * 1000;
    const uint64_t id = m_timers.Add( deadline, intervalMs * 1000, cbptr );
    RearmFd();
    return std::unique_ptr<WaylandTimer>( new WaylandTimer( shared_from_this(), id, intervalMs * 1000, cbptr ) );
}

void TimerRegistry::Shutdown()
{
    const std::scoped_lock lock( m_mutex );
    m_shuttingDown = true;
    m_timers.Clear();
    if( m_timerFd != -1 )
    {
        close( m_timerFd );
        m_timerFd = -1;
    }
}

void TimerRegistry::FireDue( uint64_t nowUs, const std::function<bool()>& keepRunning )
{
    Drain();
    try
    {
        m_timers.Fire( nowUs, keepRunning );
    }
    catch( const std::exception& ex )
    {
        mclog( LogLevel::Error, "Timer callback threw: %s", ex.what() );
    }
    catch( ... )
    {
        mclog( LogLevel::Error, "Timer callback threw unknown exception" );
    }

    const std::scoped_lock lock( m_mutex );
    if( !m_shuttingDown ) RearmFd();
}

uint64_t TimerRegistry::Rearm( uint64_t id, uint32_t delayMs, uint64_t intervalUs, const std::shared_ptr<TimerCallback>& cb )
{
    const std::scoped_lock lock( m_mutex );
    if( m_shuttingDown ) return id;
    const uint64_t deadline = GetTimeMicro() + uint64_t( delayMs ) * 1000;
    if( m_timers.Requeue( id, deadline ) )
    {
        RearmFd();
        return id;
    }
    const uint64_t newId = m_timers.Add( deadline, intervalUs, cb );
    RearmFd();
    return newId;
}

bool TimerRegistry::Stop( uint64_t id )
{
    const std::scoped_lock lock( m_mutex );
    if( m_shuttingDown ) return false;
    const bool removed = m_timers.Remove( id );
    if( removed ) RearmFd();
    return removed;
}

bool TimerRegistry::Active( uint64_t id ) const
{
    const std::scoped_lock lock( m_mutex );
    if( m_shuttingDown ) return false;
    return m_timers.Contains( id );
}

void TimerRegistry::RearmFd()
{
    if( m_timerFd == -1 ) return;
    m_timers.WithEarliest( [this]( const std::optional<uint64_t>& earliest )
    {
        if( !earliest )
        {
            const itimerspec disarm {};
            if( timerfd_settime( m_timerFd, 0, &disarm, nullptr ) != 0 ) mclog( LogLevel::Warning, "timerfd_settime disarm failed: %s", strerror( errno ) );
            return;
        }
        const itimerspec its {
            .it_interval = {},
            .it_value = timespec {
                .tv_sec = static_cast<time_t>( *earliest / 1000000 ),
                .tv_nsec = static_cast<long>( ( *earliest % 1000000 ) * 1000 ),
            },
        };
        if( timerfd_settime( m_timerFd, TFD_TIMER_ABSTIME, &its, nullptr ) != 0 ) mclog( LogLevel::Warning, "timerfd_settime failed: %s", strerror( errno ) );
    } );
}

void TimerRegistry::Drain() const
{
    if( m_timerFd == -1 ) return;
    uint64_t ticks = 0;
    ssize_t rd;
    do { rd = read( m_timerFd, &ticks, sizeof( ticks ) ); } while( rd < 0 && errno == EINTR );
}

int TimerRegistry::TimeoutMs( int minMs ) const
{
    const auto earliest = m_timers.Earliest();
    if( !earliest ) return -1;
    const uint64_t now = GetTimeMicro();
    const uint64_t us = *earliest > now ? *earliest - now : 0;
    const uint64_t ms = ( us + 999 ) / 1000;
    const int timeout = ms > static_cast<uint64_t>( INT_MAX ) ? INT_MAX : static_cast<int>( ms );
    return std::max( timeout, minMs );
}
