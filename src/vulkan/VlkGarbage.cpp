#include "VlkFence.hpp"
#include "VlkGarbage.hpp"

VlkGarbage::~VlkGarbage()
{
    for( auto& g : m_garbage ) g.fence->Wait();
}

void VlkGarbage::Add( std::shared_ptr<VlkFence> fence, std::vector<std::shared_ptr<VlkBase>>&& objects )
{
    m_garbage.emplace_back( Garbage { std::move( fence ), std::move( objects ) } );
}

void VlkGarbage::Collect()
{
    auto it = m_garbage.begin();
    while( it != m_garbage.end() )
    {
        if( it->fence->Wait( 0 ) == VK_SUCCESS )
        {
            it = m_garbage.erase( it );
        }
        else
        {
            ++it;
        }
    }
}
