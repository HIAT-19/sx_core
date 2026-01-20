#include "gtest/gtest.h"

#include <cstdint>
#include <fstream>
#include <string>

#include <unistd.h>

#include "sx/infra/infra_service.h"

namespace {

std::string MakeTempJsonPath(const std::string& name) {
    return "/tmp/sx_infra_service_" + name + "_" + std::to_string(static_cast<int64_t>(::getpid())) + ".json";
}

bool WriteFile(const std::string& path, const std::string& contents) {
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) return false;
    out << contents;
    out.close();
    return true;
}

}  // namespace

TEST(InfraService, InitAndShutdownIdempotent) {
    const std::string cfg_path = MakeTempJsonPath("cfg");
    ASSERT_TRUE(WriteFile(cfg_path, R"({"x":1})"));

    sx::infra::InfraConfig cfg;
    cfg.enable_logging = true;
    cfg.logging.log_dir = "/tmp";
    cfg.logging.file_name = "sx_infra_service_test.log";
    cfg.config_path = cfg_path;
    cfg.io_threads = 1U;
    cfg.cpu_threads = 1U;
    cfg.scheduler = nullptr;

    sx::infra::InfraService svc;
    EXPECT_FALSE(svc.init(cfg));
    EXPECT_TRUE(svc.started());

    // second call is ok
    EXPECT_FALSE(svc.init(cfg));
    EXPECT_TRUE(svc.started());

    svc.shutdown();
    EXPECT_FALSE(svc.started());
    svc.shutdown();
    EXPECT_FALSE(svc.started());
}

