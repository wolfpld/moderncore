#pragma once

#include <memory>
#include <vector>

#include "../util/NoCopy.hpp"

class Connector;

class GpuConnectors
{
public:
    GpuConnectors() = default;
    NoCopy( GpuConnectors );

    void Add( std::shared_ptr<Connector> connector );

private:
    std::vector<std::shared_ptr<Connector>> m_connectors;
};
