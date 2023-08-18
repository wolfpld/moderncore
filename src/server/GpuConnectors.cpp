#include "GpuConnectors.hpp"

void GpuConnectors::Add( std::shared_ptr<Connector> connector )
{
    m_connectors.emplace_back( std::move( connector ) );
}
