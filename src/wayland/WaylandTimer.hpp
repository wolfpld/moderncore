#pragma once

#include <memory>
#include <stdint.h>

#include "util/NoCopy.hpp"
#include "util/TimerQueue.hpp"

class TimerRegistry;

class WaylandTimer
{
public:
    ~WaylandTimer();
    NoCopy( WaylandTimer );

    void Reset( uint32_t delayMs );
    void Stop();
    [[nodiscard]] bool Active() const;

private:
    friend class TimerRegistry;
    WaylandTimer( const std::shared_ptr<TimerRegistry>& registry, uint64_t id, uint64_t intervalUs, const std::shared_ptr<TimerCallback>& cb );

    std::shared_ptr<TimerRegistry> m_registry;
    uint64_t m_id;
    uint64_t m_intervalUs;
    std::shared_ptr<TimerCallback> m_cb;
};
