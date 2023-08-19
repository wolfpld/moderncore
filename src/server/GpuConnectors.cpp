#include "Connector.hpp"
#include "GpuConnectors.hpp"

void GpuConnectors::Add( std::shared_ptr<Connector> connector )
{
    m_connectors.emplace_back( std::move( connector ) );
}

void GpuConnectors::Render()
{
    for ( auto& connector : m_connectors )
    {
        connector->Render();
    }
}
