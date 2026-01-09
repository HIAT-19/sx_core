/**
 * @file mpmc_queue.h
 * @author Luke
 * @brief 简单的多生产者多消费者队列接口，不适用于高性能场景
 * @version 0.1
 */

#pragma once

#include <mutex>
#include <queue>
#include <condition_variable>

#include "i_queue.h"

namespace sx::utils
{   

template <typename T>
class MPMCQueue : public IQueue<T>
{
private:
    mutable std::mutex mtx_;
    std::queue<T> queue_; 
    std::condition_variable cv_;

public:
    void push(T item) noexcept override
    {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            queue_.push(std::move(item));
        }
        cv_.notify_one();
    }

    void wait_and_pop(T& item) noexcept override
    {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this]() { return !queue_.empty(); });
        item = std::move(queue_.front());
        queue_.pop();
    }

    [[nodiscard]] std::shared_ptr<T> wait_and_pop() noexcept override
    {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this]() { return !queue_.empty(); });
        auto item = std::make_shared<T>(std::move(queue_.front()));
        queue_.pop();
        return item;
    }

    [[nodiscard]] bool try_pop(T& item) noexcept override
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (queue_.empty())
        {
            return false;
        }
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    [[nodiscard]] std::shared_ptr<T> try_pop() noexcept override
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (queue_.empty())
        {
            return nullptr;
        }
        auto item = std::make_shared<T>(std::move(queue_.front()));
        queue_.pop();
        return item;
    }

    [[nodiscard]] bool empty() const noexcept override
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.empty();
    }

    MPMCQueue() = default;
    virtual ~MPMCQueue() = default;
    MPMCQueue(const MPMCQueue&) = delete;
    MPMCQueue& operator=(const MPMCQueue&) = delete;
    MPMCQueue(MPMCQueue&&) = delete;
    MPMCQueue& operator=(MPMCQueue&&) = delete;
};

}  // namespace sx::utils