#pragma once

#include <cstddef>
#include <stdint.h>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

#include "util/NoCopy.hpp"

using TimerCallback = std::function<void()>;

class TimerQueue
{
public:
    TimerQueue() = default;
    NoCopy( TimerQueue );

    uint64_t Add( uint64_t deadlineUs, uint64_t intervalUs, std::shared_ptr<TimerCallback> cb );
    [[nodiscard]] bool Remove( uint64_t id );
    [[nodiscard]] bool Requeue( uint64_t id, uint64_t deadlineUs );
    size_t Fire( uint64_t nowUs, const std::function<bool()>& keepRunning = {} );

    void Clear();
    [[nodiscard]] bool Contains( uint64_t id ) const;

    [[nodiscard]] std::optional<uint64_t> Earliest() const;

    // Invokes f with the earliest deadline while holding the queue lock, so an
    // arm-the-fd-to-this-value action cannot race a concurrent mutation.
    template<class F>
    void WithEarliest( F&& f )
    {
        const std::scoped_lock lock( m_lock );
        f( EarliestLocked() );
    }

private:
    struct Entry;

    static bool LaterThan( const std::shared_ptr<Entry>& a, const std::shared_ptr<Entry>& b );
    void Push( std::shared_ptr<Entry> e );
    void EraseFromHeap( const std::shared_ptr<Entry>& e );
    void Finalize( const std::shared_ptr<Entry>& e, uint64_t nowUs );
    std::optional<uint64_t> EarliestLocked() const;

    std::vector<std::shared_ptr<Entry>> m_heap;
    std::unordered_map<uint64_t, std::shared_ptr<Entry>> m_revMap;
    uint64_t m_nextId = 1;
    mutable std::mutex m_lock;
};
