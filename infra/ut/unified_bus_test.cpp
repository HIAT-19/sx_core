#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <thread>

#include <unistd.h>

#include "sx/infra/unified_bus.h"
#include "sx/types/unified_bus_types.h"

namespace {

std::string UniqueSuffix() {
    static std::atomic<uint64_t> counter{0};
    const uint64_t c = counter.fetch_add(1, std::memory_order_relaxed);
    return std::to_string(static_cast<int64_t>(::getpid())) + "_" + std::to_string(static_cast<uint64_t>(c));
}

std::string MakeInprocEndpoint(const std::string& name) {
    // Scheme A: control-plane "topic" is the endpoint string.
    // Note: inproc requires bind before connect; our UnifiedBus binds on first publish().
    return "inproc://sx_ut_" + name + "_" + UniqueSuffix();
}

std::string MakeDataTopic(const std::string& name) {
    return "ut.data." + name + "." + UniqueSuffix();
}

}  // namespace

TEST(UnifiedBusDataPlane, MultipleSubscribersSameTopicBroadcast) {
    auto& bus = sx::infra::UnifiedBus::get_instance();
    const std::string topic = MakeDataTopic("broadcast");

    auto fifo_q = bus.subscribe_stream<int>(topic, sx::types::StreamMode::kReliableFifo);
    auto latest_q = bus.subscribe_stream<int>(topic, sx::types::StreamMode::kRealTimeLatest);

    bus.publish_stream<int>(topic, std::make_shared<int>(1));
    bus.publish_stream<int>(topic, std::make_shared<int>(2));

    std::shared_ptr<int> out;
    ASSERT_TRUE(fifo_q->try_pop(out));
    ASSERT_TRUE(out);
    EXPECT_EQ(*out, 1);

    ASSERT_TRUE(fifo_q->try_pop(out));
    ASSERT_TRUE(out);
    EXPECT_EQ(*out, 2);

    ASSERT_TRUE(latest_q->try_pop(out));
    ASSERT_TRUE(out);
    EXPECT_EQ(*out, 2);
}

TEST(UnifiedBusControlPlane, InprocPublishSubscribeEventuallyDelivers) {
    auto& bus = sx::infra::UnifiedBus::get_instance();
    const std::string endpoint = MakeInprocEndpoint("ctrl_basic");

    // Bind first (inproc requires bind before connect).
    ASSERT_FALSE(bus.publish(endpoint, "warmup"));

    std::promise<std::string> got;
    std::future<std::string> fut = got.get_future();
    std::atomic<bool> once{false};

    ASSERT_FALSE(bus.subscribe(endpoint, [&](const std::string& msg) {
        if (!once.exchange(true, std::memory_order_relaxed)) {
            got.set_value(msg);
        }
    }));

    // PUB/SUB has "slow joiner" behavior; retry until received or timeout.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline) {
        (void)bus.publish(endpoint, "hello");
        if (fut.wait_for(std::chrono::milliseconds(20)) == std::future_status::ready) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    ASSERT_EQ(fut.wait_for(std::chrono::milliseconds(0)), std::future_status::ready);
    EXPECT_EQ(fut.get(), "hello");
}

TEST(UnifiedBusControlPlane, InprocMultipleCallbacksSameEndpoint) {
    auto& bus = sx::infra::UnifiedBus::get_instance();
    const std::string endpoint = MakeInprocEndpoint("ctrl_multi");

    ASSERT_FALSE(bus.publish(endpoint, "warmup"));

    std::promise<std::string> got1;
    std::promise<std::string> got2;
    auto fut1 = got1.get_future();
    auto fut2 = got2.get_future();
    std::atomic<bool> once1{false};
    std::atomic<bool> once2{false};

    ASSERT_FALSE(bus.subscribe(endpoint, [&](const std::string& msg) {
        if (!once1.exchange(true, std::memory_order_relaxed)) got1.set_value(msg);
    }));
    ASSERT_FALSE(bus.subscribe(endpoint, [&](const std::string& msg) {
        if (!once2.exchange(true, std::memory_order_relaxed)) got2.set_value(msg);
    }));

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline) {
        (void)bus.publish(endpoint, "ping");
        const bool ok1 = (fut1.wait_for(std::chrono::milliseconds(10)) == std::future_status::ready);
        const bool ok2 = (fut2.wait_for(std::chrono::milliseconds(10)) == std::future_status::ready);
        if (ok1 && ok2) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    ASSERT_EQ(fut1.wait_for(std::chrono::milliseconds(0)), std::future_status::ready);
    ASSERT_EQ(fut2.wait_for(std::chrono::milliseconds(0)), std::future_status::ready);
    EXPECT_EQ(fut1.get(), "ping");
    EXPECT_EQ(fut2.get(), "ping");
}


