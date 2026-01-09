#pragma once

#include <memory>

namespace sx::utils {

template <typename T>
class IQueue {
public:
    virtual void push(T item) noexcept = 0;

    virtual void wait_and_pop(T& item) noexcept = 0;

    [[nodiscard]] virtual std::shared_ptr<T> wait_and_pop() noexcept = 0;

    [[nodiscard]] virtual bool try_pop(T& item) noexcept = 0;

    [[nodiscard]] virtual std::shared_ptr<T> try_pop() noexcept = 0;

    [[nodiscard]] virtual bool empty() const noexcept = 0;

    virtual ~IQueue() = default;

    IQueue() = default;
    IQueue(const IQueue&) = delete;
    IQueue& operator=(const IQueue&) = delete;
    IQueue(IQueue&&) = delete;
    IQueue& operator=(IQueue&&) = delete;
};

}  // namespace sx::utils


