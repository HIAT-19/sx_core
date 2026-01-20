/**
 * @file logging.cpp
 * @brief LogManager implementation (spdlog)
 */

#include "sx/infra/logging.h"

#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>

namespace sx::infra {

namespace {

spdlog::level::level_enum ToSpdLevel(LogLevel lvl) {
    switch (lvl) {
        case LogLevel::kTrace: return spdlog::level::trace;
        case LogLevel::kDebug: return spdlog::level::debug;
        case LogLevel::kInfo: return spdlog::level::info;
        case LogLevel::kWarn: return spdlog::level::warn;
        case LogLevel::kError: return spdlog::level::err;
        case LogLevel::kCritical: return spdlog::level::critical;
        case LogLevel::kOff: return spdlog::level::off;
    }
    return spdlog::level::info;
}

}  // namespace

class SpdLogger final : public ILogger {
public:
    explicit SpdLogger(std::shared_ptr<spdlog::logger> impl) : impl_(std::move(impl)) {}

    void log(LogLevel level, std::string_view msg) override {
        if (!impl_) return;
        impl_->log(ToSpdLevel(level), "{}", msg);
    }

private:
    std::shared_ptr<spdlog::logger> impl_;
};

struct LogManager::Impl {
    std::mutex mutex;
    bool inited = false;

    LoggingConfig cfg;
    std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> sink;

    // Cache of named loggers.
    std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> loggers;

    // Per-logger level overrides (applied when created / updated on set_level).
    std::unordered_map<std::string, LogLevel> levels;

    std::shared_ptr<spdlog::logger> get_or_create_logger_locked(const std::string& name) {
        auto it = loggers.find(name);
        if (it != loggers.end()) return it->second;

        auto logger = std::make_shared<spdlog::logger>(name, sink);
        logger->set_level(ToSpdLevel(cfg.default_level));
        logger->flush_on(spdlog::level::err);

        if (!cfg.pattern.empty()) {
            logger->set_pattern(cfg.pattern);
        }

        if (auto lit = levels.find(name); lit != levels.end()) {
            logger->set_level(ToSpdLevel(lit->second));
        }

        loggers.emplace(name, logger);
        return logger;
    }
};

LogManager::LogManager() : pImpl_(std::make_unique<Impl>()) {}
LogManager::~LogManager() { shutdown(); }

std::error_code LogManager::init(const LoggingConfig& cfg) {
    std::lock_guard<std::mutex> lock(pImpl_->mutex);
    if (pImpl_->inited) return {};

    pImpl_->cfg = cfg;

    std::error_code ec;
    std::filesystem::create_directories(pImpl_->cfg.log_dir, ec);
    if (ec) return ec;

    const std::filesystem::path file_path = std::filesystem::path(pImpl_->cfg.log_dir) / pImpl_->cfg.file_name;

    try {
        pImpl_->sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            file_path.string(), pImpl_->cfg.max_size_bytes, pImpl_->cfg.max_files);
    } catch (...) {
        return std::make_error_code(std::errc::io_error);
    }

    pImpl_->inited = true;
    return {};
}

std::shared_ptr<ILogger> LogManager::get_logger(const std::string& name) {
    std::lock_guard<std::mutex> lock(pImpl_->mutex);

    // Lazy-init with defaults if user didn't call init().
    if (!pImpl_->inited) {
        const LoggingConfig defaults;
        (void)init(defaults);
    }

    auto spd = pImpl_->get_or_create_logger_locked(name);
    return std::make_shared<SpdLogger>(std::move(spd));
}

void LogManager::set_level(const std::string& logger_name, LogLevel level) {
    std::lock_guard<std::mutex> lock(pImpl_->mutex);
    pImpl_->levels[logger_name] = level;

    auto it = pImpl_->loggers.find(logger_name);
    if (it != pImpl_->loggers.end() && it->second) {
        it->second->set_level(ToSpdLevel(level));
    }
}

void LogManager::set_default_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(pImpl_->mutex);
    pImpl_->cfg.default_level = level;
}

void LogManager::flush() {
    std::lock_guard<std::mutex> lock(pImpl_->mutex);
    for (auto& [name, lg] : pImpl_->loggers) {
        (void)name;
        if (lg) lg->flush();
    }
}

void LogManager::shutdown() {
    std::lock_guard<std::mutex> lock(pImpl_->mutex);
    if (!pImpl_->inited) return;

    // Drop references.
    pImpl_->loggers.clear();
    pImpl_->levels.clear();
    pImpl_->sink.reset();
    pImpl_->inited = false;
}

}  // namespace sx::infra


