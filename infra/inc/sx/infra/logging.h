#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <system_error>

namespace sx::infra {

enum class LogLevel {
    kTrace,
    kDebug,
    kInfo,
    kWarn,
    kError,
    kCritical,
    kOff,
};

struct LoggingConfig {
    // Directory for log files.
    std::string log_dir = "/tmp";

    // Base filename inside log_dir, e.g. "sx.log" => "/tmp/sx.log".
    std::string file_name = "sx.log";

    // Rotating file parameters.
    std::size_t max_size_bytes = 10U * 1024U * 1024U;  // 10MB
    std::size_t max_files = 3U;

    LogLevel default_level = LogLevel::kInfo;

    // spdlog pattern string (kept as plain string in header).
    // Example: "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v"
    std::string pattern;
};

class ILogger {
public:
    virtual ~ILogger() = default;

    virtual void log(LogLevel level, std::string_view msg) = 0;

    void trace(std::string_view msg) { log(LogLevel::kTrace, msg); }
    void debug(std::string_view msg) { log(LogLevel::kDebug, msg); }
    void info(std::string_view msg) { log(LogLevel::kInfo, msg); }
    void warn(std::string_view msg) { log(LogLevel::kWarn, msg); }
    void error(std::string_view msg) { log(LogLevel::kError, msg); }
    void critical(std::string_view msg) { log(LogLevel::kCritical, msg); }
};

class LogManager {
public:
    LogManager();
    ~LogManager();

    // Initialize global sinks / defaults. Idempotent.
    std::error_code init(const LoggingConfig& cfg);

    // Get or create a named logger (module).
    std::shared_ptr<ILogger> get_logger(const std::string& name);

    // Module-level control (\"enable only one module\" is achieved by setting
    // others to kOff and the target to desired level).
    void set_level(const std::string& logger_name, LogLevel level);
    void set_default_level(LogLevel level);

    void flush();
    void shutdown();

    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;
    LogManager(LogManager&&) = delete;
    LogManager& operator=(LogManager&&) = delete;

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace sx::infra


