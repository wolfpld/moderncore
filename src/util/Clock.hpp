#pragma once

#include <chrono>

[[maybe_unused]] static inline uint64_t GetTimeMicro()
{
    return std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::steady_clock::now().time_since_epoch() ).count();
}
