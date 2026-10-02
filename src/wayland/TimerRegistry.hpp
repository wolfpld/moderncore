#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdint.h>

#include "util/NoCopy.hpp"
#include "util/TimerQueue.hpp"

class WaylandTimer;

class TimerRegistry : public std::enable_shared_from_this<TimerRegistry>
{
public:
    TimerRegistry();
    ~TimerRegistry();
    NoCopy( TimerRegistry );

    [[nodiscard]] std::unique_ptr<WaylandTimer> Add( uint32_t delayMs, uint64_t intervalMs, TimerCallback cb );
    void Shutdown();
    void FireDue( uint64_t nowUs, const std::function<bool()>& keepRunning );

    [[nodiscard]] int PollFd() const { return m_timerFd; }
    [[nodiscard]] int TimeoutMs( int minMs ) const;
    [[nodiscard]] std::optional<uint64_t> Earliest() const { return m_timers.Earliest(); }

private:
    friend class WaylandTimer;

    void Drain() const;
    uint64_t Rearm( uint64_t id, uint32_t delayMs, uint64_t intervalUs, const std::shared_ptr<TimerCallback>& cb );
    bool Stop( uint64_t id );
    bool Active( uint64_t id ) const;

    void RearmFd();

    TimerQueue m_timers;
    int m_timerFd = -1;
    mutable std::mutex m_mutex;
    bool m_shuttingDown = false;
};
