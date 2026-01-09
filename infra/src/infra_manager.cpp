/**
 * @file infra_manager.cpp
 * @brief InfraManager implementation
 */

#include "sx/infra/infra_manager.h"

#include <mutex>

#include "sx/infra/async_runtime.h"
#include "sx/infra/config_manager.h"
#include "sx/infra/logging.h"
#include "sx/infra/unified_bus.h"

namespace sx::infra {

namespace {

std::mutex& InfraMutex() {
    static std::mutex m;
    return m;
}

bool& InfraStarted() {
    static bool started = false;
    return started;
}

}  // namespace

std::error_code InfraManager::init_all(const InfraConfig& cfg) {
    std::lock_guard<std::mutex> lock(InfraMutex());
    if (InfraStarted()) return {};

    // 1) Logging first (so subsequent infra can log if needed)
    if (cfg.enable_logging) {
        const std::error_code ec = LogManager::instance().init(cfg.logging);
        if (ec) return ec;
    }

    // 2) Async runtime (threads, scheduler, timers/strands)
    AsyncRuntime::instance().init(cfg.scheduler, cfg.io_threads, cfg.cpu_threads);

    // 3) Config (optional)
    if (!cfg.config_path.empty()) {
        const std::error_code ec = ConfigManager::instance().load(cfg.config_path);
        if (ec) {
            // Leave runtime running; caller may decide to shutdown_all().
            return ec;
        }
    }

    // 4) Ensure bus singleton is constructed (no-op otherwise)
    (void)UnifiedBus::get_instance();

    InfraStarted() = true;
    return {};
}

void InfraManager::shutdown_all() {
    std::lock_guard<std::mutex> lock(InfraMutex());
    if (!InfraStarted()) return;

    // Reverse order. UnifiedBus currently has no explicit stop API; it is
    // cleaned up at process exit. AsyncRuntime is stoppable.
    AsyncRuntime::instance().stop();

    // Logging (optional)
    LogManager::instance().shutdown();

    InfraStarted() = false;
}

}  // namespace sx::infra


