#include <common/TracySystem.hpp>
#include <tracy/Tracy.hpp>

#include "VlkBase.hpp"
#include "VlkFence.hpp"
#include "VlkGarbage.hpp"
#include "util/Logs.hpp"

VlkGarbage::VlkGarbage()
    : m_shutdown( false )
    , m_reaper( [this] { Reaper(); } )
{
}

VlkGarbage::~VlkGarbage()
{
    m_shutdown.store( true, std::memory_order_release );
    m_cv.notify_all();
    m_reaper.join();

    if( m_garbage.empty() ) return;
    mclog( LogLevel::Debug, "Waiting for %zu garbage sets", m_garbage.size() );
    for( auto& g : m_garbage ) g.first->Wait();
}

void VlkGarbage::Recycle( std::shared_ptr<VlkFence> fence, std::shared_ptr<VlkBase>&& object )
{
    std::lock_guard lock( m_lock );
    auto it = m_garbage.find( fence );
    if( it != m_garbage.end() )
    {
        it->second.emplace_back( std::move( object ) );
    }
    else
    {
        m_garbage.emplace( std::move( fence ), std::vector<std::shared_ptr<VlkBase>> { std::move( object ) } );
    }
    m_cv.notify_one();
}

void VlkGarbage::Recycle( std::shared_ptr<VlkFence> fence, std::vector<std::shared_ptr<VlkBase>>&& objects )
{
    std::lock_guard lock( m_lock );
    auto it = m_garbage.find( fence );
    if( it != m_garbage.end() )
    {
        it->second.insert( it->second.end(), objects.begin(), objects.end() );
    }
    else
    {
        m_garbage.emplace( std::move( fence ), std::move( objects ) );
    }
    m_cv.notify_one();
}

void VlkGarbage::Collect()
{
    std::lock_guard lock( m_lock );
    ReapGarbage();
}

void VlkGarbage::Reaper()
{
    tracy::SetThreadName( "VlkGarbage reaper" );

    while( !m_shutdown.load( std::memory_order_acquire ) )
    {
        std::unique_lock lock( m_lock );
        m_cv.wait( lock, [this] { return m_shutdown.load( std::memory_order_acquire ) || !m_garbage.empty(); } );
        if( m_shutdown.load( std::memory_order_acquire ) ) return;

        ReapGarbage();

        if( !m_garbage.empty() )
        {
            mclog( LogLevel::Debug, "Garbage is still present, sleeping for 100 ms" );
            m_cv.wait_for( lock, std::chrono::milliseconds( 100 ) );
        }
    }
}

void VlkGarbage::ReapGarbage()
{
    ZoneScoped;

    size_t count = 0;
    size_t sets = 0;

    auto it = m_garbage.begin();
    while( it != m_garbage.end() )
    {
        if( it->first->Wait( 0 ) == VK_SUCCESS )
        {
            count += it->second.size();
            sets++;
            it = m_garbage.erase( it );
        }
        else
        {
            ++it;
        }
    }

    if( count > 0 ) mclog( LogLevel::Debug, "Garbage collected: %zu sets, %zu items", sets, count );
}
