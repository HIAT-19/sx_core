#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <system_error>
#include <type_traits>
#include <utility>

#include "sx/hal/i_thread_scheduler.h"
#include "sx/types/thread_policy.h"

namespace sx::infra {

class ITimer {
public:
    virtual ~ITimer() = default;

    virtual void expires_after(std::chrono::milliseconds timeout) = 0;
    virtual void async_wait(std::function<void(const std::error_code&)> callback) = 0;
    virtual void cancel() = 0;
};

class IExecutor {
public:
    virtual ~IExecutor() = default;
    virtual void post(std::function<void()> f) = 0;
};

class AsyncRuntime {
public:
    static AsyncRuntime& instance();
    ~AsyncRuntime();

    // Inject a platform thread scheduler (nullable). Starts IO/CPU worker threads.
    void init(std::shared_ptr<sx::hal::IThreadScheduler> scheduler, std::size_t io_n = 2U,
              std::size_t cpu_n = 4U);

    // Stop all loops and join threads. Safe to call multiple times.
    void stop();

    template <typename Func>
    void post_io(Func&& f) {
        post_io_impl(std::function<void()>(std::forward<Func>(f)));
    }

    template <typename Func>
    void post_cpu(Func&& f) {
        post_cpu_impl(std::function<void()>(std::forward<Func>(f)));
    }

    // Resource factory
    std::shared_ptr<ITimer> create_timer();
    std::shared_ptr<IExecutor> create_cpu_strand();
    std::shared_ptr<IExecutor> create_io_strand();

    // Escape hatch: start a managed dedicated loop thread.
    // If Func is invocable with (std::atomic<bool>&), it will receive a stop flag.
    // Otherwise Func() will be called.
    template <typename Func>
    void spawn_critical_loop(const sx::types::ThreadPolicy& policy, Func&& f) {
        if constexpr (std::is_invocable_v<Func, std::atomic<bool>&>) {
            spawn_critical_loop_impl(
                policy, std::function<void(std::atomic<bool>&)>(std::forward<Func>(f)));
        } else {
            spawn_critical_loop_impl(policy, [fn = std::forward<Func>(f)](std::atomic<bool>&) mutable { fn(); });
        }
    }

private:
    AsyncRuntime();
    AsyncRuntime(const AsyncRuntime&) = delete;
    AsyncRuntime& operator=(const AsyncRuntime&) = delete;
    AsyncRuntime(AsyncRuntime&&) = delete;
    AsyncRuntime& operator=(AsyncRuntime&&) = delete;

    void post_io_impl(std::function<void()> f);
    void post_cpu_impl(std::function<void()> f);
    void spawn_critical_loop_impl(const sx::types::ThreadPolicy& policy,
                                  std::function<void(std::atomic<bool>&)> f);

    struct Impl;
    std::unique_ptr<Impl> pImpl_;

    // Reserved for HAL adapters that need direct access to the underlying IO context.
    // Use requires explicit approval.
    friend class AsioSerial;
    void* internal_get_io_context();
};

}  // namespace sx::infra


