/**
 * @file async_runtime.cpp
 * @brief AsyncRuntime implementation (standalone Asio)
 */

#include "sx/infra/async_runtime.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include <asio.hpp>

namespace sx::infra {

class AsioTimer final : public ITimer {
public:
    explicit AsioTimer(asio::io_context& ctx) : timer_(ctx) {}

    void expires_after(std::chrono::milliseconds timeout) override { timer_.expires_after(timeout); }

    void async_wait(std::function<void(const std::error_code&)> callback) override {
        timer_.async_wait([cb = std::move(callback)](const std::error_code& ec) {
            if (cb) cb(ec);
        });
    }

    void cancel() override {
        (void)timer_.cancel();
    }

private:
    asio::steady_timer timer_;
};

class AsioExecutor final : public IExecutor {
public:
    explicit AsioExecutor(asio::io_context& ctx) : strand_(asio::make_strand(ctx)) {}

    void post(std::function<void()> f) override { asio::post(strand_, std::move(f)); }

private:
    asio::strand<asio::io_context::executor_type> strand_;
};

struct AsyncRuntime::Impl {
    std::mutex mutex_;
    bool started_ = false;

    asio::io_context io_ctx_;
    asio::io_context cpu_ctx_;

    std::optional<asio::executor_work_guard<asio::io_context::executor_type>> io_work_;
    std::optional<asio::executor_work_guard<asio::io_context::executor_type>> cpu_work_;

    std::vector<std::thread> io_threads_;
    std::vector<std::thread> cpu_threads_;
    std::vector<std::thread> critical_threads_;

    std::atomic<bool> stop_{false};
    std::shared_ptr<sx::hal::IThreadScheduler> scheduler_;

    void start_threads_locked(std::size_t io_n, std::size_t cpu_n) {
        io_threads_.reserve(io_n);
        cpu_threads_.reserve(cpu_n);

        for (std::size_t i = 0; i < io_n; ++i) {
            io_threads_.emplace_back([this, i]() {
                if (scheduler_) scheduler_->on_thread_start(sx::hal::IThreadScheduler::ThreadClass::kIo, i);
                io_ctx_.run();
            });
        }

        for (std::size_t i = 0; i < cpu_n; ++i) {
            cpu_threads_.emplace_back([this, i]() {
                if (scheduler_) scheduler_->on_thread_start(sx::hal::IThreadScheduler::ThreadClass::kCpu, i);
                cpu_ctx_.run();
            });
        }
    }

    void stop_and_join_locked() {
        stop_.store(true, std::memory_order_relaxed);

        if (io_work_) io_work_.reset();
        if (cpu_work_) cpu_work_.reset();

        io_ctx_.stop();
        cpu_ctx_.stop();

        for (auto& t : io_threads_) {
            if (t.joinable()) t.join();
        }
        for (auto& t : cpu_threads_) {
            if (t.joinable()) t.join();
        }
        for (auto& t : critical_threads_) {
            if (t.joinable()) t.join();
        }

        io_threads_.clear();
        cpu_threads_.clear();
        critical_threads_.clear();

        // Prepare contexts for potential re-init.
        io_ctx_.restart();
        cpu_ctx_.restart();

        started_ = false;
    }
};

AsyncRuntime::AsyncRuntime() : pImpl_(std::make_unique<Impl>()) {}
AsyncRuntime::~AsyncRuntime() { stop(); }

void AsyncRuntime::init(std::shared_ptr<sx::hal::IThreadScheduler> scheduler, std::size_t io_n, std::size_t cpu_n) {
    if (io_n == 0U) io_n = 1U;
    if (cpu_n == 0U) cpu_n = std::max<std::size_t>(1U, std::thread::hardware_concurrency());

    std::lock_guard<std::mutex> lock(pImpl_->mutex_);
    if (pImpl_->started_) return;

    pImpl_->stop_.store(false, std::memory_order_relaxed);
    pImpl_->scheduler_ = std::move(scheduler);

    pImpl_->io_work_.emplace(asio::make_work_guard(pImpl_->io_ctx_));
    pImpl_->cpu_work_.emplace(asio::make_work_guard(pImpl_->cpu_ctx_));

    pImpl_->start_threads_locked(io_n, cpu_n);
    pImpl_->started_ = true;
}

void AsyncRuntime::stop() {
    std::lock_guard<std::mutex> lock(pImpl_->mutex_);
    if (!pImpl_->started_) return;
    pImpl_->stop_and_join_locked();
}

void AsyncRuntime::post_io_impl(std::function<void()> f) {
    std::lock_guard<std::mutex> lock(pImpl_->mutex_);
    if (!pImpl_->started_) return;
    asio::post(pImpl_->io_ctx_, std::move(f));
}

void AsyncRuntime::post_cpu_impl(std::function<void()> f) {
    std::lock_guard<std::mutex> lock(pImpl_->mutex_);
    if (!pImpl_->started_) return;
    asio::post(pImpl_->cpu_ctx_, std::move(f));
}

std::shared_ptr<ITimer> AsyncRuntime::create_timer() {
    std::lock_guard<std::mutex> lock(pImpl_->mutex_);
    assert(pImpl_->started_ && "AsyncRuntime::init() must be called before create_timer()");
    return std::make_shared<AsioTimer>(pImpl_->io_ctx_);
}

std::shared_ptr<IExecutor> AsyncRuntime::create_cpu_strand() {
    std::lock_guard<std::mutex> lock(pImpl_->mutex_);
    assert(pImpl_->started_ && "AsyncRuntime::init() must be called before create_cpu_strand()");
    return std::make_shared<AsioExecutor>(pImpl_->cpu_ctx_);
}

std::shared_ptr<IExecutor> AsyncRuntime::create_io_strand() {
    std::lock_guard<std::mutex> lock(pImpl_->mutex_);
    assert(pImpl_->started_ && "AsyncRuntime::init() must be called before create_io_strand()");
    return std::make_shared<AsioExecutor>(pImpl_->io_ctx_);
}

void AsyncRuntime::spawn_critical_loop_impl(const sx::types::ThreadPolicy& policy,
                                           std::function<void(std::atomic<bool>&)> f) {
    std::lock_guard<std::mutex> lock(pImpl_->mutex_);
    if (!pImpl_->started_) return;

    // The critical loop shares the runtime stop flag. Business code should check it.
    pImpl_->critical_threads_.emplace_back([this, policy, fn = std::move(f)]() mutable {
        if (pImpl_->scheduler_) {
            pImpl_->scheduler_->on_thread_start(sx::hal::IThreadScheduler::ThreadClass::kCritical, 0U);
            pImpl_->scheduler_->apply_current_thread_policy(policy);
        }
        if (fn) fn(pImpl_->stop_);
    });
}

void* AsyncRuntime::internal_get_io_context() {
    return static_cast<void*>(&pImpl_->io_ctx_);
}

}  // namespace sx::infra


