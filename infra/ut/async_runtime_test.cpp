#include "gtest/gtest.h"

#include <chrono>
#include <future>
#include <memory>
#include <vector>

#include "sx/infra/async_runtime.h"

TEST(AsyncRuntime, PostIoExecutes) {
    auto& rt = sx::infra::AsyncRuntime::instance();
    rt.stop();
    rt.init(nullptr, 1U, 1U);

    std::promise<int> done;
    auto fut = done.get_future();

    rt.post_io([&done]() { done.set_value(123); });
    ASSERT_EQ(fut.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    EXPECT_EQ(fut.get(), 123);

    rt.stop();
}

TEST(AsyncRuntime, PostCpuExecutes) {
    auto& rt = sx::infra::AsyncRuntime::instance();
    rt.stop();
    rt.init(nullptr, 1U, 2U);

    std::promise<void> done;
    auto fut = done.get_future();

    rt.post_cpu([&done]() { done.set_value(); });
    ASSERT_EQ(fut.wait_for(std::chrono::seconds(1)), std::future_status::ready);

    rt.stop();
}

TEST(AsyncRuntime, TimerFiresOnIoPool) {
    auto& rt = sx::infra::AsyncRuntime::instance();
    rt.stop();
    rt.init(nullptr, 1U, 1U);

    auto timer = rt.create_timer();
    ASSERT_TRUE(timer);

    std::promise<std::error_code> got;
    auto fut = got.get_future();

    timer->expires_after(std::chrono::milliseconds(10));
    timer->async_wait([&got](const std::error_code& ec) { got.set_value(ec); });

    ASSERT_EQ(fut.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    const auto ec = fut.get();
    EXPECT_FALSE(ec);

    rt.stop();
}

TEST(AsyncRuntime, CpuStrandSerializes) {
    auto& rt = sx::infra::AsyncRuntime::instance();
    rt.stop();
    rt.init(nullptr, 1U, 4U);

    auto ex = rt.create_cpu_strand();
    ASSERT_TRUE(ex);

    auto seq = std::make_shared<std::vector<int>>();
    seq->reserve(100);

    std::promise<void> done;
    auto fut = done.get_future();

    for (int i = 0; i < 100; ++i) {
        ex->post([seq, i]() { seq->push_back(i); });
    }
    ex->post([&done]() { done.set_value(); });

    ASSERT_EQ(fut.wait_for(std::chrono::seconds(2)), std::future_status::ready);

    ASSERT_EQ(seq->size(), 100U);
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ((*seq)[static_cast<std::size_t>(i)], i);
    }

    rt.stop();
}


