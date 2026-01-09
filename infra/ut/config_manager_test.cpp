#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>

#include "sx/infra/config_manager.h"

namespace {

std::string UniqueSuffix() {
    static std::atomic<uint64_t> counter{0};
    const uint64_t c = counter.fetch_add(1, std::memory_order_relaxed);
    return std::to_string(static_cast<int64_t>(::getpid())) + "_" + std::to_string(static_cast<uint64_t>(c));
}

std::string MakeTempJsonPath(const std::string& name) {
    return "/tmp/sx_cfg_" + name + "_" + UniqueSuffix() + ".json";
}

bool WriteFile(const std::string& path, const std::string& contents) {
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) return false;
    out << contents;
    out.close();
    return true;
}

}  // namespace

TEST(ConfigManager, BasicTypesDotPathAndDefaults) {
    auto& cfg = sx::infra::ConfigManager::instance();
    const std::string path = MakeTempJsonPath("basic");

    ASSERT_TRUE(WriteFile(
        path,
        R"({
  "ai": { "yolo": { "threshold": 0.7, "enabled": true, "name": "v8" } },
  "port": 5555,
  "paths": { "device": "/dev/video0" },
  "cameras": [ { "ip": "10.0.0.1" }, { "ip": "10.0.0.2" } ],
  "numbers": [1,2,3],
  "floats": [1.5,2.5],
  "strings": ["a","b"]
})"));

    ASSERT_FALSE(cfg.load(path));

    EXPECT_EQ(cfg.get<int>("port", 0), 5555);
    EXPECT_NEAR(cfg.get<double>("ai.yolo.threshold", 0.0), 0.7, 1e-9);
    EXPECT_TRUE(cfg.get<bool>("ai.yolo.enabled", false));
    EXPECT_EQ(cfg.get<std::string>("paths.device", ""), "/dev/video0");
    EXPECT_EQ(cfg.get<std::string>("cameras.0.ip", ""), "10.0.0.1");
    EXPECT_EQ(cfg.get<std::string>("cameras.1.ip", ""), "10.0.0.2");

    const auto nums = cfg.get<std::vector<int>>("numbers", {});
    ASSERT_EQ(nums.size(), 3U);
    EXPECT_EQ(nums[0], 1);
    EXPECT_EQ(nums[1], 2);
    EXPECT_EQ(nums[2], 3);

    const auto floats = cfg.get<std::vector<float>>("floats", {});
    ASSERT_EQ(floats.size(), 2U);
    EXPECT_NEAR(floats[0], 1.5F, 1e-6F);
    EXPECT_NEAR(floats[1], 2.5F, 1e-6F);

    const auto strs = cfg.get<std::vector<std::string>>("strings", {});
    ASSERT_EQ(strs.size(), 2U);
    EXPECT_EQ(strs[0], "a");
    EXPECT_EQ(strs[1], "b");

    // Missing path -> default
    EXPECT_EQ(cfg.get<int>("no.such.key", 123), 123);
    // Type mismatch -> default
    EXPECT_EQ(cfg.get<int>("ai.yolo.name", 456), 456);
}

TEST(ConfigManager, ReloadNotifiesListeners) {
    auto& cfg = sx::infra::ConfigManager::instance();
    const std::string path = MakeTempJsonPath("reload");

    ASSERT_TRUE(WriteFile(path, R"({"x":1})"));
    ASSERT_FALSE(cfg.load(path));
    EXPECT_EQ(cfg.get<int>("x", 0), 1);

    auto count = std::make_shared<std::atomic<int>>(0);
    cfg.register_listener("x", [count]() { count->fetch_add(1, std::memory_order_relaxed); });

    ASSERT_TRUE(WriteFile(path, R"({"x":2})"));
    ASSERT_FALSE(cfg.reload());

    EXPECT_EQ(cfg.get<int>("x", 0), 2);
    EXPECT_GE(count->load(std::memory_order_relaxed), 1);
}

TEST(ConfigManager, ThreadSafeReadsDuringReload) {
    auto& cfg = sx::infra::ConfigManager::instance();
    const std::string path = MakeTempJsonPath("thread");

    ASSERT_TRUE(WriteFile(path, R"({"x":1,"arr":[1,2,3]})"));
    ASSERT_FALSE(cfg.load(path));

    std::atomic<bool> stop{false};
    std::atomic<bool> ok{true};

    auto reader = [&]() {
        while (!stop.load(std::memory_order_relaxed)) {
            const int x = cfg.get<int>("x", -1);
            const auto arr = cfg.get<std::vector<int>>("arr", {});
            if (x != 1 && x != 2) ok.store(false, std::memory_order_relaxed);
            if (arr.size() != 3U) ok.store(false, std::memory_order_relaxed);
            if (!arr.empty() && (arr[0] != 1 || arr[1] != 2 || arr[2] != 3)) ok.store(false, std::memory_order_relaxed);
        }
    };

    std::thread t1(reader);
    std::thread t2(reader);
    std::thread t3(reader);

    for (int i = 0; i < 20; ++i) {
        const int v = (i % 2 == 0) ? 2 : 1;
        ASSERT_TRUE(WriteFile(path, std::string(R"({"x":)") + std::to_string(v) + R"(,"arr":[1,2,3]})"));
        ASSERT_FALSE(cfg.reload());
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    stop.store(true, std::memory_order_relaxed);
    t1.join();
    t2.join();
    t3.join();

    EXPECT_TRUE(ok.load(std::memory_order_relaxed));
}


