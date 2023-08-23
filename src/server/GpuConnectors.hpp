#pragma once

#include <atomic>
#include <memory>

#include "../util/NoCopy.hpp"
#include "../util/RobinHood.hpp"

class Connector;

class GpuConnectors
{
public:
    GpuConnectors() = default;
    NoCopy( GpuConnectors );

    uint32_t Add( std::shared_ptr<Connector> connector );
    void Remove( uint32_t id );

    void Render();

private:
    unordered_flat_map<uint32_t, std::shared_ptr<Connector>> m_connectors;

    std::atomic<uint32_t> m_id = 0;
};
