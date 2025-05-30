#include <stdio.h>
#include <tracy/Tracy.hpp>

#include "TaskDispatch.hpp"
#include "util/Logs.hpp"

TaskDispatch::TaskDispatch( size_t workers, const char* name )
    : m_exit( false )
    , m_jobs( 0 )
    , m_initDone( false )
{
    ZoneScoped;

    m_init = std::thread( [this, workers, name] {
        mclog( LogLevel::Info, "Creating %zu worker threads named '%s'", workers, name );

        m_workers.reserve( workers );
        for( size_t i=0; i<workers; i++ )
        {
            m_workers.emplace_back( [this, name, i]{ SetName( name, i ); Worker(); } );
        }
    } );
}

TaskDispatch::~TaskDispatch()
{
    WaitInit();

    m_exit.store( true, std::memory_order_release );
    m_queueLock.lock();
    m_cvWork.notify_all();
    m_queueLock.unlock();

    for( auto& worker : m_workers )
    {
        worker.join();
    }
}

void TaskDispatch::WaitInit()
{
    if( m_initDone.load( std::memory_order_relaxed ) ) return;

    std::lock_guard lock( m_initLock );
    if( !m_initDone.load( std::memory_order_acquire ) )
    {
        m_init.join();
        m_initDone.store( true, std::memory_order_release );
    }
}

void TaskDispatch::Queue( const std::function<void(void)>& f )
{
    std::lock_guard lock( m_queueLock );
    m_queue.emplace_back( f );
    m_cvWork.notify_one();
}

void TaskDispatch::Queue( std::function<void(void)>&& f )
{
    std::lock_guard lock( m_queueLock );
    m_queue.emplace_back( std::move( f ) );
    m_cvWork.notify_one();
}

void TaskDispatch::Sync()
{
    std::unique_lock lock( m_queueLock );
    while( !m_queue.empty() )
    {
        auto f = m_queue.back();
        m_queue.pop_back();
        lock.unlock();
        f();
        lock.lock();
    }
    m_cvJobs.wait( lock, [this]{ return m_jobs == 0; } );
}

void TaskDispatch::Worker()
{
    for(;;)
    {
        std::unique_lock lock( m_queueLock );
        m_cvWork.wait( lock, [this]{ return !m_queue.empty() || m_exit.load( std::memory_order_acquire ); } );
        if( m_exit.load( std::memory_order_acquire ) ) return;
        auto f = m_queue.back();
        m_queue.pop_back();
        m_jobs++;
        lock.unlock();
        f();
        lock.lock();
        m_jobs--;
        if( m_jobs == 0 && m_queue.empty() ) m_cvJobs.notify_all();
        lock.unlock();
    }
}

void TaskDispatch::SetName( const char* name, size_t num )
{
    char tmp[128];
    snprintf( tmp, sizeof( tmp ), "%s #%zu", name, num );
    tracy::SetThreadName( tmp );
}
