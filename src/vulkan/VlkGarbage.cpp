#include "VlkFence.hpp"
#include "VlkGarbage.hpp"
#include "util/Logs.hpp"

VlkGarbage::~VlkGarbage()
{
    if( m_garbage.empty() ) return;
    mclog( LogLevel::Debug, "Waiting for %zu garbage items", m_garbage.size() );
    for( auto& g : m_garbage ) g.fence->Wait();
}

void VlkGarbage::Recycle( std::shared_ptr<VlkFence> fence, std::vector<std::shared_ptr<VlkBase>>&& objects )
{
    m_garbage.emplace_back( Garbage { std::move( fence ), std::move( objects ) } );
}

void VlkGarbage::Collect()
{
    size_t count = 0;

    auto it = m_garbage.begin();
    while( it != m_garbage.end() )
    {
        if( it->fence->Wait( 0 ) == VK_SUCCESS )
        {
            it = m_garbage.erase( it );
            ++count;
        }
        else
        {
            ++it;
        }
    }

    if( count > 0 ) mclog( LogLevel::Debug, "Garbage collected: %zu", count );
}
