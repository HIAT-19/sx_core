#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <system_error>

#include "sx/hal/i_thread_scheduler.h"
#include "sx/infra/logging.h"

namespace sx::infra {

class ConfigManager;
class AsyncRuntime;
class UnifiedBus;

struct InfraConfig {
    // Logging (optional)
    bool enable_logging = false;
    LoggingConfig logging;

    // Config (optional)
    std::string config_path;

    // AsyncRuntime pools
    std::size_t io_threads = 2U;
    std::size_t cpu_threads = 0U;  // 0 => use hardware_concurrency()

    // Optional platform scheduler for affinity / priority.
    std::shared_ptr<sx::hal::IThreadScheduler> scheduler;
};

// DI-friendly container for infra components (no singletons).
class InfraService {
public:
    InfraService();
    ~InfraService();

    InfraService(const InfraService&) = delete;
    InfraService& operator=(const InfraService&) = delete;
    InfraService(InfraService&&) = delete;
    InfraService& operator=(InfraService&&) = delete;

    // Initialize in a consistent order. Idempotent.
    std::error_code init(const InfraConfig& cfg);

    // Shutdown in reverse order. Idempotent.
    void shutdown();

    [[nodiscard]] bool started() const noexcept { return started_; }

    LogManager& logging() { return logging_; }
    ConfigManager& config();
    AsyncRuntime& runtime();
    UnifiedBus& bus();

private:
    bool started_ = false;
    InfraConfig cfg_;

    LogManager logging_;
    std::unique_ptr<ConfigManager> config_;
    std::unique_ptr<AsyncRuntime> runtime_;
    std::unique_ptr<UnifiedBus> bus_;
};

}  // namespace sx::infra

