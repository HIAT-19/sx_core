/**
 * @file config_manager.cpp
 * @brief ConfigManager implementation
 */

#include "sx/infra/config_manager.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <type_traits>
#include <utility>

#include <nlohmann/json.hpp>  

namespace sx::infra {

namespace {

[[nodiscard]] bool IsUnsignedIntegerToken(const std::string& s) {
    if (s.empty()) return false;
    return std::all_of(s.begin(), s.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; });
}

[[nodiscard]] bool ParseSizeTNoExcept(const std::string& s, std::size_t& out) {
    if (!IsUnsignedIntegerToken(s)) return false;

    std::size_t value = 0;
    for (const char ch0 : s) {
        const auto ch = static_cast<unsigned char>(ch0);
        const auto digit = static_cast<std::size_t>(ch - static_cast<unsigned char>('0'));
        if (value > (std::numeric_limits<std::size_t>::max() - digit) / 10U) return false;
        value = value * 10U + digit;
    }
    out = value;
    return true;
}

[[nodiscard]] bool ReadFileToString(const std::string& path, std::string& out) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::ostringstream oss;
    oss << file.rdbuf();
    out = oss.str();
    return true;
}

[[nodiscard]] bool ParseJsonNoExcept(const std::string& text, nlohmann::json& out) {
    out = nlohmann::json::parse(text, nullptr, /*allow_exceptions=*/false);
    return !out.is_discarded();
}

template <typename T>
[[nodiscard]] bool JsonToValueNoExcept(const nlohmann::json& node, T& out);

[[nodiscard]] bool JsonToIntNoExcept(const nlohmann::json& node, int& out) {
    if (!(node.is_number_integer() || node.is_number_unsigned())) return false;
    const auto v = node.get<nlohmann::json::number_integer_t>();
    if (v < static_cast<nlohmann::json::number_integer_t>(std::numeric_limits<int>::min())) return false;
    if (v > static_cast<nlohmann::json::number_integer_t>(std::numeric_limits<int>::max())) return false;
    out = static_cast<int>(v);
    return true;
}

[[nodiscard]] bool JsonToFloatNoExcept(const nlohmann::json& node, float& out) {
    if (!node.is_number()) return false;
    out = static_cast<float>(node.get<nlohmann::json::number_float_t>());
    return true;
}

[[nodiscard]] bool JsonToDoubleNoExcept(const nlohmann::json& node, double& out) {
    if (!node.is_number()) return false;
    out = static_cast<double>(node.get<nlohmann::json::number_float_t>());
    return true;
}

[[nodiscard]] bool JsonToBoolNoExcept(const nlohmann::json& node, bool& out) {
    if (!node.is_boolean()) return false;
    out = node.get<bool>();
    return true;
}

[[nodiscard]] bool JsonToStringNoExcept(const nlohmann::json& node, std::string& out) {
    if (!node.is_string()) return false;
    out = node.get<std::string>();
    return true;
}

template <typename ElemT>
[[nodiscard]] bool JsonArrayToVectorNoExcept(const nlohmann::json& node, std::vector<ElemT>& out) {
    if (!node.is_array()) return false;
    std::vector<ElemT> v;
    v.reserve(node.size());
    for (const auto& el : node) {
        ElemT item{};
        if (!JsonToValueNoExcept<ElemT>(el, item)) return false;
        v.push_back(std::move(item));
    }
    out = std::move(v);
    return true;
}

template <typename T>
[[nodiscard]] bool JsonToValueNoExcept(const nlohmann::json& node, T& out) {
    if constexpr (std::is_same_v<T, int>) {
        return JsonToIntNoExcept(node, out);
    } else if constexpr (std::is_same_v<T, float>) {
        return JsonToFloatNoExcept(node, out);
    } else if constexpr (std::is_same_v<T, double>) {
        return JsonToDoubleNoExcept(node, out);
    } else if constexpr (std::is_same_v<T, bool>) {
        return JsonToBoolNoExcept(node, out);
    } else if constexpr (std::is_same_v<T, std::string>) {
        return JsonToStringNoExcept(node, out);
    } else if constexpr (std::is_same_v<T, std::vector<int>>) {
        return JsonArrayToVectorNoExcept(node, out);
    } else if constexpr (std::is_same_v<T, std::vector<float>>) {
        return JsonArrayToVectorNoExcept(node, out);
    } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
        return JsonArrayToVectorNoExcept(node, out);
    } else {
        static_assert(sizeof(T) == 0, "Unsupported type for ConfigManager::get<T>");
    }
}

}  // namespace

struct ConfigManager::Impl {
    nlohmann::json root;
    std::string config_path;

    mutable std::shared_mutex mutex;
    std::map<std::string, std::vector<UpdateCallback>> listeners;

    const nlohmann::json* traverse(const std::string& path) const {
        const nlohmann::json* curr = &root;

        std::string token;
        std::istringstream tokenStream(path);
        while (std::getline(tokenStream, token, '.')) {
            if (token.empty()) return nullptr;

            // 1) Object key
            if (curr->is_object()) {
                auto it = curr->find(token);
                if (it != curr->end()) {
                    curr = &(*it);
                    continue;
                }
            }

            // 2) Array index
            if (curr->is_array()) {
                std::size_t idx = 0;
                if (!ParseSizeTNoExcept(token, idx)) return nullptr;
                const auto uidx = static_cast<nlohmann::json::size_type>(idx);
                if (uidx >= curr->size()) return nullptr;
                curr = &((*curr)[uidx]);
                continue;
            }

            return nullptr;
        }

        return curr;
    }
};

ConfigManager::ConfigManager() : pImpl_(std::make_unique<Impl>()) {}
ConfigManager::~ConfigManager() = default;

std::error_code ConfigManager::load(const std::string& path) {
    std::string text;
    if (!ReadFileToString(path, text)) return std::make_error_code(std::errc::no_such_file_or_directory);

    nlohmann::json new_root;
    if (!ParseJsonNoExcept(text, new_root)) return std::make_error_code(std::errc::illegal_byte_sequence);

    {
        std::unique_lock lock(pImpl_->mutex);
        pImpl_->config_path = path;
        pImpl_->root = std::move(new_root);
    }

    return {};
}

std::error_code ConfigManager::reload() {
    std::string path;
    {
        std::shared_lock lock(pImpl_->mutex);
        path = pImpl_->config_path;
    }

    if (path.empty()) return std::make_error_code(std::errc::invalid_argument);

    std::string text;
    if (!ReadFileToString(path, text)) return std::make_error_code(std::errc::no_such_file_or_directory);

    nlohmann::json new_root;
    if (!ParseJsonNoExcept(text, new_root)) return std::make_error_code(std::errc::illegal_byte_sequence);

    std::vector<UpdateCallback> callbacks;
    {
        std::unique_lock lock(pImpl_->mutex);
        pImpl_->root = std::move(new_root);

        // Snapshot callbacks to call without holding locks.
        for (const auto& [key, cbs] : pImpl_->listeners) {
            (void)key;
            callbacks.insert(callbacks.end(), cbs.begin(), cbs.end());
        }
    }

    for (auto& cb : callbacks) {
        if (cb) cb();
    }

    return {};
}

void ConfigManager::register_listener(const std::string& key_path, UpdateCallback cb) {
    if (!cb) return;
    std::unique_lock lock(pImpl_->mutex);
    pImpl_->listeners[key_path].push_back(std::move(cb));
}

template <typename T>
T ConfigManager::get(const std::string& key_path, const T& default_val) const {
    std::shared_lock lock(pImpl_->mutex);
    const auto* node = pImpl_->traverse(key_path);
    if (node == nullptr || node->is_null()) return default_val;

    T out = default_val;
    if (!JsonToValueNoExcept<T>(*node, out)) return default_val;
    return out;
}

// Explicit instantiations (compile firewall)
template int ConfigManager::get<int>(const std::string&, const int&) const;
template float ConfigManager::get<float>(const std::string&, const float&) const;
template double ConfigManager::get<double>(const std::string&, const double&) const;
template bool ConfigManager::get<bool>(const std::string&, const bool&) const;
template std::string ConfigManager::get<std::string>(const std::string&, const std::string&) const;

template std::vector<int> ConfigManager::get<std::vector<int>>(
    const std::string&, const std::vector<int>&) const;
template std::vector<float> ConfigManager::get<std::vector<float>>(
    const std::string&, const std::vector<float>&) const;
template std::vector<std::string> ConfigManager::get<std::vector<std::string>>(
    const std::string&, const std::vector<std::string>&) const;

}  // namespace sx::infra


