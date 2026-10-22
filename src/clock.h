#pragma once

#include <chrono>
#include <cstdint>

namespace nanoredis {

// Milliseconds from a monotonic clock; unaffected by wall-clock changes.
inline uint64_t monotonic_ms() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

}  // namespace nanoredis
