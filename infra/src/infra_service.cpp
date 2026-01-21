/**
 * @file infra_service.cpp
 * @brief InfraService implementation
 */

#include "sx/infra/infra_service.h"

#include <cassert>

#include "sx/infra/async_runtime.h"
#include "sx/infra/config_manager.h"
#include "sx/infra/unified_bus.h"

namespace sx::infra {

InfraService::InfraService() = default;
InfraService::~InfraService() { shutdown(); }

ConfigManager& InfraService::config() {
    assert(config_);
    return *config_;
}

AsyncRuntime& InfraService::runtime() {
    assert(runtime_);
    return *runtime_;
}

UnifiedBus& InfraService::bus() {
    assert(bus_);
    return *bus_;
}

LogManager& InfraService::logging() {
    assert(logging_);
    return *logging_;
}

std::error_code InfraService::init(const InfraConfig& cfg) {
    if (started_) return {};

    cfg_ = cfg;

    // 1) Logging
    if (cfg_.enable_logging) {
        if (!logging_) logging_ = std::make_unique<LogManager>();
        if (const auto ec = logging_->init(cfg_.logging)) return ec;
    }

    // 2) Runtime
    if (!runtime_) runtime_ = std::make_unique<AsyncRuntime>();
    runtime_->init(cfg_.scheduler, cfg_.io_threads, cfg_.cpu_threads);

    // 3) Config (optional)
    if (!cfg_.config_path.empty()) {
        if (!config_) config_ = std::make_unique<ConfigManager>();
        if (const auto ec = config_->load(cfg_.config_path)) return ec;
    }

    // 4) Bus
    if (!bus_) bus_ = std::make_unique<UnifiedBus>();
    started_ = true;
    return {};
}

void InfraService::shutdown() {
    if (!started_) return;

    // Reverse order
    if (bus_) bus_->shutdown();
    if (runtime_) runtime_->stop();
    if (logging_) logging_->shutdown();

    started_ = false;
}

}  // namespace sx::infra

