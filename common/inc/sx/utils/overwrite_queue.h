/**
 * @file overwrite_queue.h
 * @author Luke
 * @brief 简单的多生产者多消费者队列接口，满时会覆盖旧数据，不适用于高性能场景
 * @version 0.1
 */

#pragma once

#include "i_queue.h"
#include <mutex>
#include <memory>
#include <thread>

namespace sx::utils
{

template <typename T>
class OverwriteQueue : public IQueue<T>
{
private:
    mutable std::mutex mtx_;
    std::shared_ptr<T> data_; // 存储最新的一帧数据
    bool has_data_ = false;

public:
    explicit OverwriteQueue(size_t /*capacity*/ = 1) {} 

    void push(T item) noexcept override
    {
        std::lock_guard<std::mutex> lock(mtx_);
        data_ = std::make_shared<T>(std::move(item));
        has_data_ = true;
    }

    void wait_and_pop(T& item) noexcept override
    {
        while (true) {
            if (try_pop(item)) return;
            std::this_thread::yield(); 
        }
    }

    [[nodiscard]] std::shared_ptr<T> wait_and_pop() noexcept override
    {
        while(true) {
            auto ptr = try_pop();
            if (ptr) return ptr;
            std::this_thread::yield();
        }
    }

    [[nodiscard]] bool try_pop(T& item) noexcept override
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!has_data_) return false;
        item = *data_;
        has_data_ = false; 
        return true;
    }

    [[nodiscard]] std::shared_ptr<T> try_pop() noexcept override
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!has_data_) return nullptr;
        auto ptr = std::make_shared<T>(*data_); 
        has_data_ = false;
        return ptr;
    }

    [[nodiscard]] bool empty() const noexcept override
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return !has_data_;
    }
    
    virtual ~OverwriteQueue() = default;
    OverwriteQueue(const OverwriteQueue&) = delete;
    OverwriteQueue& operator=(const OverwriteQueue&) = delete;
    OverwriteQueue(OverwriteQueue&&) = delete;
    OverwriteQueue& operator=(OverwriteQueue&&) = delete;
};
} // namespace sx::utils
