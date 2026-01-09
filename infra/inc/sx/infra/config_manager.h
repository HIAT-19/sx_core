#pragma once

#include <functional>
#include <memory>
#include <string>
#include <system_error>

namespace sx::infra {

class ConfigManager {
public:
    using UpdateCallback = std::function<void()>;

    // Singleton
    static ConfigManager& instance();

    // Must be defined in .cpp due to Pimpl unique_ptr incomplete type rule.
    ~ConfigManager();

    // =========================================================
    // Init & Reload
    // =========================================================

    // Load JSON config file from disk.
    std::error_code load(const std::string& path);

    // Hot reload from the last loaded path and notify listeners.
    std::error_code reload();

    // =========================================================
    // Core read API (thread-safe)
    // =========================================================

    // NOTE:
    // - This template is intentionally defined in the .cpp (compile firewall).
    // - Only a small set of types are explicitly instantiated (see .cpp).
    template <typename T>
    [[nodiscard]] T get(const std::string& key_path, const T& default_val) const;

    // =========================================================
    // Change notifications
    // =========================================================

    // Register listener for key_path. On reload(), callbacks will be invoked
    // (without holding internal locks). Business code should call get() again.
    void register_listener(const std::string& key_path, UpdateCallback cb);

    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    ConfigManager(ConfigManager&&) = delete;
    ConfigManager& operator=(ConfigManager&&) = delete;

private:
    ConfigManager();

    struct Impl;
    std::unique_ptr<Impl> pImpl_;
};

// Prevent implicit instantiation in users (keeps JSON out of headers and speeds builds).
extern template int ConfigManager::get<int>(const std::string&, const int&) const;
extern template float ConfigManager::get<float>(const std::string&, const float&) const;
extern template double ConfigManager::get<double>(const std::string&, const double&) const;
extern template bool ConfigManager::get<bool>(const std::string&, const bool&) const;
extern template std::string ConfigManager::get<std::string>(const std::string&, const std::string&) const;

extern template std::vector<int> ConfigManager::get<std::vector<int>>(
    const std::string&, const std::vector<int>&) const;
extern template std::vector<float> ConfigManager::get<std::vector<float>>(
    const std::string&, const std::vector<float>&) const;
extern template std::vector<std::string> ConfigManager::get<std::vector<std::string>>(
    const std::string&, const std::vector<std::string>&) const;

}  // namespace sx::infra


