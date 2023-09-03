#include <assert.h>

#include "Connector.hpp"
#include "GpuConnectors.hpp"

uint32_t GpuConnectors::Add( std::shared_ptr<Connector> connector )
{
    const auto id = m_id.fetch_add( 1, std::memory_order_relaxed );
    m_connectors.emplace( id, std::move( connector ) );
    return id;
}

void GpuConnectors::Remove( uint32_t id )
{
    assert( m_connectors.find( id ) != m_connectors.end() );
    m_connectors.erase( id );
}

void GpuConnectors::Render( const std::vector<std::shared_ptr<Renderable>>& renderables )
{
    for ( auto& connector : m_connectors )
    {
        connector.second->Render( renderables );
    }
}
