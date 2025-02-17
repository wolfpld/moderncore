#include "VlkBase.hpp"
#include "VlkFence.hpp"
#include "VlkGarbage.hpp"
#include "util/Logs.hpp"

VlkGarbage::~VlkGarbage()
{
    if( m_garbage.empty() ) return;
    mclog( LogLevel::Debug, "Waiting for %zu garbage sets", m_garbage.size() );
    for( auto& g : m_garbage ) g.first->Wait();
}

void VlkGarbage::Recycle( std::shared_ptr<VlkFence> fence, std::shared_ptr<VlkBase>&& object )
{
    auto it = m_garbage.find( fence );
    if( it != m_garbage.end() )
    {
        it->second.emplace_back( std::move( object ) );
    }
    else
    {
        m_garbage.emplace( std::move( fence ), std::vector<std::shared_ptr<VlkBase>> { std::move( object ) } );
    }
}

void VlkGarbage::Recycle( std::shared_ptr<VlkFence> fence, std::vector<std::shared_ptr<VlkBase>>&& objects )
{
    auto it = m_garbage.find( fence );
    if( it != m_garbage.end() )
    {
        it->second.insert( it->second.end(), objects.begin(), objects.end() );
    }
    else
    {
        m_garbage.emplace( std::move( fence ), std::move( objects ) );
    }
}

void VlkGarbage::Collect()
{
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
