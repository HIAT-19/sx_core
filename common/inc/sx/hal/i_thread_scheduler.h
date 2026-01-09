#pragma once

#include <cstddef>
#include <memory>

#include "sx/types/thread_policy.h"

namespace sx::hal {

// Optional platform hook for thread affinity / priority control.
// Default usage: pass nullptr into AsyncRuntime::init() to disable.
class IThreadScheduler {
public:
    enum class ThreadClass {
        kIo,
        kCpu,
        kCritical,
    };

    virtual ~IThreadScheduler() = default;

    // Called at the beginning of each worker thread (inside the thread).
    virtual void on_thread_start(ThreadClass cls, std::size_t index) = 0;

    // Called inside critical loop thread to apply a policy (optional).
    virtual void apply_current_thread_policy(const sx::types::ThreadPolicy& policy) = 0;
};

}  // namespace sx::hal


