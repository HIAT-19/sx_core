#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <system_error>

#include "sx/hal/i_thread_scheduler.h"
#include "sx/infra/logging.h"

namespace sx::infra {

struct InfraConfig {
    // If true, LogManager::init() will be called before other infra components.
    bool enable_logging = false;
    LoggingConfig logging;

    // If empty, ConfigManager::load() will be skipped.
    std::string config_path;

    // AsyncRuntime thread pool sizes.
    std::size_t io_threads = 2U;
    std::size_t cpu_threads = 0U;  // 0 => use hardware_concurrency()

    // Optional platform scheduler for affinity / priority.
    std::shared_ptr<sx::hal::IThreadScheduler> scheduler;
};

class InfraManager {
public:
    // Initialize infra components in a consistent order.
    // Safe to call multiple times (idempotent).
    static std::error_code init_all(const InfraConfig& cfg);

    // Shutdown infra components in reverse order.
    // Safe to call multiple times (idempotent).
    static void shutdown_all();
};

}  // namespace sx::infra


