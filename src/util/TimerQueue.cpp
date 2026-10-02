#include <algorithm>
#include <atomic>
#include <utility>

#include "util/Panic.hpp"
#include "util/TimerQueue.hpp"

struct TimerQueue::Entry
{
    uint64_t id = 0;
    uint64_t deadline = 0;
    uint64_t interval = 0;
    bool inFlight = false;
    std::atomic<bool> cancelled = false;
    std::atomic<bool> requeued = false;
    std::shared_ptr<TimerCallback> cb;
};

bool TimerQueue::LaterThan( const std::shared_ptr<Entry>& a, const std::shared_ptr<Entry>& b )
{
    const uint64_t da = a->deadline;
    const uint64_t db = b->deadline;
    if( da != db ) return da > db;
    return a->id > b->id;
}

void TimerQueue::Push( std::shared_ptr<Entry> e )
{
    m_heap.push_back( std::move( e ) );
    std::ranges::push_heap( m_heap, LaterThan );
}

void TimerQueue::EraseFromHeap( const std::shared_ptr<Entry>& e )
{
    const auto it = std::ranges::find( m_heap, e );
    if( it == m_heap.end() ) return;
    m_heap.erase( it );
    std::ranges::make_heap( m_heap, LaterThan );
}

uint64_t TimerQueue::Add( uint64_t deadlineUs, uint64_t intervalUs, std::shared_ptr<TimerCallback> cb )
{
    auto e = std::make_shared<Entry>();
    const std::scoped_lock lock( m_lock );
    e->id = m_nextId++;
    e->interval = intervalUs;
    e->deadline = deadlineUs;
    e->cb = std::move( cb );
    m_revMap[e->id] = e;
    Push( e );
    return e->id;
}

bool TimerQueue::Remove( uint64_t id )
{
    const std::scoped_lock lock( m_lock );
    const auto it = m_revMap.find( id );
    if( it == m_revMap.end() ) return false;
    const auto& e = it->second;
    e->cancelled = true;
    if( !e->inFlight ) EraseFromHeap( e );
    m_revMap.erase( it );
    return true;
}

bool TimerQueue::Requeue( uint64_t id, uint64_t deadlineUs )
{
    const std::scoped_lock lock( m_lock );
    const auto it = m_revMap.find( id );
    if( it == m_revMap.end() ) return false;
    const auto& e = it->second;
    if( e->cancelled ) return false;
    if( e->inFlight )
    {
        e->deadline = deadlineUs;
        e->requeued = true;
        return true;
    }
    EraseFromHeap( e );
    e->deadline = deadlineUs;
    Push( e );
    return true;
}

void TimerQueue::Finalize( const std::shared_ptr<Entry>& e, uint64_t nowUs )
{
    const std::scoped_lock lock( m_lock );
    if( !e->cancelled && e->requeued )
    {
        e->requeued = false;
        e->inFlight = false;
        Push( e );
    }
    else if( !e->cancelled && e->interval > 0 )
    {
        CheckPanic( e->interval <= UINT64_MAX - nowUs, "Repeating timer rearm overflows the deadline clock" );
        e->deadline = nowUs + e->interval;
        e->inFlight = false;
        Push( e );
    }
    else
    {
        m_revMap.erase( e->id );
    }
}

size_t TimerQueue::Fire( uint64_t nowUs, const std::function<bool()>& keepRunning )
{
    std::vector<std::shared_ptr<Entry>> batch;
    {
        const std::scoped_lock lock( m_lock );
        while( !m_heap.empty() && m_heap.front()->deadline <= nowUs )
        {
            std::ranges::pop_heap( m_heap, LaterThan );
            std::shared_ptr<Entry> e = std::move( m_heap.back() );
            m_heap.pop_back();
            e->inFlight = true;
            batch.push_back( std::move( e ) );
        }
    }

    size_t fired = 0;
    bool discardRest = false;
    size_t idx = 0;
    try
    {
        while( idx < batch.size() )
        {
            const std::shared_ptr<Entry>& e = batch[idx];
            bool invoke = !discardRest && !e->cancelled && !e->requeued;
            if( invoke && keepRunning && !keepRunning() )
            {
                invoke = false;
                discardRest = true;
            }
            if( invoke )
            {
                (*e->cb)();
                fired++;
            }
            else if( !e->requeued )
            {
                e->cancelled = true;
            }
            Finalize( e, nowUs );
            idx++;
        }
    }
    catch( ... )
    {
        while( idx < batch.size() )
        {
            batch[idx]->cancelled = true;
            Finalize( batch[idx], nowUs );
            idx++;
        }
        throw;
    }
    return fired;
}

void TimerQueue::Clear()
{
    const std::scoped_lock lock( m_lock );
    for( auto& [id, e]: m_revMap ) e->cancelled = true;
    m_revMap.clear();
    m_heap.clear();
}

bool TimerQueue::Contains( uint64_t id ) const
{
    const std::scoped_lock lock( m_lock );
    return m_revMap.contains( id );
}

std::optional<uint64_t> TimerQueue::Earliest() const
{
    const std::scoped_lock lock( m_lock );
    return EarliestLocked();
}

std::optional<uint64_t> TimerQueue::EarliestLocked() const
{
    std::optional<uint64_t> best = {};
    for( const auto& [id, e]: m_revMap )
    {
        if( e->cancelled || ( e->inFlight && !e->requeued ) ) continue;
        if( !best || e->deadline < *best ) best = e->deadline;
    }
    return best;
}
