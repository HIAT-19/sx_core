#pragma once

#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

#include "sx/types/unified_bus_types.h"
#include "sx/utils/i_queue.h"

namespace sx::infra
{

// 队列句柄
template <typename U>
using StreamQueuePtr = std::shared_ptr<sx::utils::IQueue<std::shared_ptr<U>>>;

// 类型擦除适配器：将底层的 shared_ptr<void> 队列适配为 shared_ptr<T> 队列
template <typename T>
class TypedQueueAdapter : public sx::utils::IQueue<std::shared_ptr<T>>
{
public:
    explicit TypedQueueAdapter(std::shared_ptr<sx::utils::IQueue<std::shared_ptr<void>>> impl)
        : impl_(std::move(impl))
    {
    }

    void push(std::shared_ptr<T> item) noexcept override
    {
        impl_->push(std::static_pointer_cast<void>(item));
    }

    void wait_and_pop(std::shared_ptr<T>& item) noexcept override
    {
        std::shared_ptr<void> void_ptr;
        impl_->wait_and_pop(void_ptr);
        item = std::static_pointer_cast<T>(void_ptr);
    }

    std::shared_ptr<std::shared_ptr<T>> wait_and_pop() noexcept override
    {
        auto void_ptr_ptr = impl_->wait_and_pop();
        if (!void_ptr_ptr) return nullptr;
        return std::make_shared<std::shared_ptr<T>>(std::static_pointer_cast<T>(*void_ptr_ptr));
    }

    bool try_pop(std::shared_ptr<T>& item) noexcept override
    {
        std::shared_ptr<void> void_ptr;
        if (impl_->try_pop(void_ptr)) {
            item = std::static_pointer_cast<T>(void_ptr);
            return true;
        }
        return false;
    }

    std::shared_ptr<std::shared_ptr<T>> try_pop() noexcept override
    {
        auto void_ptr_ptr = impl_->try_pop();
        if (!void_ptr_ptr) return nullptr;
        return std::make_shared<std::shared_ptr<T>>(std::static_pointer_cast<T>(*void_ptr_ptr));
    }

    [[nodiscard]] bool empty() const noexcept override { return impl_->empty(); }

private:
    std::shared_ptr<sx::utils::IQueue<std::shared_ptr<void>>> impl_;
};

class UnifiedBus
{
public:
    UnifiedBus();
    ~UnifiedBus();

    // ================================ Publish ================================

    /**
     * @brief 发布控制消息，路由至 ZeroMQ
     * 适用于：状态、指令、小数据
     */
    [[nodiscard]] std::error_code publish(const std::string& topic, const std::string& message);

    /**
     * @brief 发布二进制数据，路由至内存队列 (Zero-Copy)
     * 适用于：大文件、图像、视频等
     * @param data 必须是 shared_ptr，强制零拷贝语义
     */
    template <typename T>
    void publish_stream(const std::string& topic, std::shared_ptr<T> data)
    {
        publish_stream_impl(topic, std::static_pointer_cast<void>(data));
    }

    // ================================ Subscribe ================================

    /**
     * @brief 订阅控制消息，回调模式
     */
    [[nodiscard]] std::error_code subscribe(const std::string& topic,
                                            std::function<void(const std::string&)> callback);

    // Explicit shutdown for deterministic teardown (threads, zmq context).
    void shutdown();

    /**
     * @brief 订阅二进制数据，队列句柄模式
     * @return 返回队列句柄，消费者直接从队列 pop 数据
     */
    template <typename T>
    StreamQueuePtr<T> subscribe_stream(const std::string& topic, sx::types::StreamMode mode)
    {
        auto void_queue = std::static_pointer_cast<sx::utils::IQueue<std::shared_ptr<void>>>(
            subscribe_stream_impl(topic, mode));
        return std::make_shared<TypedQueueAdapter<T>>(void_queue);
    }

    UnifiedBus(const UnifiedBus&) = delete;
    UnifiedBus& operator=(const UnifiedBus&) = delete;
    UnifiedBus(UnifiedBus&&) = delete;
    UnifiedBus& operator=(UnifiedBus&&) = delete;

private:
    // 辅助的非模板接口，用于 Pimpl 桥接
    void publish_stream_impl(const std::string& topic, std::shared_ptr<void> data);

    // 返回的是 shared_ptr<IQueue<shared_ptr<void>>>
    // 但为了避免在头文件引入过多 shared_ptr 嵌套定义，这里用 shared_ptr<void> 作为返回值类型擦除，
    // 在模板实现里再强转。
    std::shared_ptr<void> subscribe_stream_impl(const std::string& topic,
                                                sx::types::StreamMode mode);

    // Pimpl 实现
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace sx::infra
