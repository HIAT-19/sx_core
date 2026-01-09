#include "gtest/gtest.h"

#include <cstdint>
#include <chrono>
#include <fstream>
#include <string>
#include <thread>

#include <unistd.h>

#include "sx/infra/logging.h"

namespace {

std::string TempDir() { return "/tmp/sx_log_ut_" + std::to_string(static_cast<int64_t>(::getpid())); }

std::string ReadAll(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return {};
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

}  // namespace

TEST(Logging, PerLoggerLevelControl) {
    sx::infra::LoggingConfig cfg;
    cfg.log_dir = TempDir();
    cfg.file_name = "sx_test.log";
    cfg.max_size_bytes = 1024U * 1024U;
    cfg.max_files = 2U;
    cfg.default_level = sx::infra::LogLevel::kInfo;
    cfg.pattern = "[%n] [%l] %v";

    auto& mgr = sx::infra::LogManager::instance();
    mgr.shutdown();

    ASSERT_FALSE(mgr.init(cfg));

    auto a = mgr.get_logger("a");
    auto b = mgr.get_logger("b");
    ASSERT_TRUE(a);
    ASSERT_TRUE(b);

    // Enable only logger "a" for debug.
    mgr.set_level("a", sx::infra::LogLevel::kDebug);
    mgr.set_level("b", sx::infra::LogLevel::kOff);

    a->debug("a_debug");
    b->info("b_info");
    b->error("b_error");

    mgr.flush();

    const std::string path = cfg.log_dir + "/" + cfg.file_name;

    // Best-effort wait for FS flush.
    for (int i = 0; i < 50; ++i) {
        const std::string text = ReadAll(path);
        if (!text.empty()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    const std::string text = ReadAll(path);
    EXPECT_NE(text.find("a_debug"), std::string::npos);
    EXPECT_EQ(text.find("b_info"), std::string::npos);
    EXPECT_EQ(text.find("b_error"), std::string::npos);

    mgr.shutdown();
}


