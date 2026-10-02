#include "WaylandTimer.hpp"
#include "TimerRegistry.hpp"

WaylandTimer::WaylandTimer( const std::shared_ptr<TimerRegistry>& registry, uint64_t id, uint64_t intervalUs, const std::shared_ptr<TimerCallback>& cb )
    : m_registry( registry )
    , m_id( id )
    , m_intervalUs( intervalUs )
    , m_cb( cb )
{
}

WaylandTimer::~WaylandTimer()
{
    m_registry->Stop( m_id );
}

void WaylandTimer::Reset( uint32_t delayMs )
{
    m_id = m_registry->Rearm( m_id, delayMs, m_intervalUs, m_cb );
}

void WaylandTimer::Stop()
{
    m_registry->Stop( m_id );
}

bool WaylandTimer::Active() const
{
    return m_registry->Active( m_id );
}
