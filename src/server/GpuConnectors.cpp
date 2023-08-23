#include "Connector.hpp"
#include "GpuConnectors.hpp"

uint32_t GpuConnectors::Add( std::shared_ptr<Connector> connector )
{
    const auto id = m_id.fetch_add( 1, std::memory_order_relaxed );
    m_connectors.emplace( id, std::move( connector ) );
    return id;
}

void GpuConnectors::Render()
{
    for ( auto& connector : m_connectors )
    {
        connector.second->Render();
    }
}
