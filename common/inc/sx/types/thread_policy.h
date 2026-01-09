#pragma once

#include <cstdint>

namespace sx::types {

// Minimal thread policy for embedded platforms.
// Platform layer (HAL) may interpret these fields as needed.
struct ThreadPolicy {
    // -1 means "no preference"
    int cpu_id = -1;

    // -1 means "do not change"
    // For Linux SCHED_FIFO, typical range is 1..99 (platform dependent).
    int realtime_priority = -1;

    // Whether to attempt realtime scheduling (platform dependent).
    bool realtime = false;
};

}  // namespace sx::types


