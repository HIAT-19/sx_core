## sx_core

Core infrastructure library for embedded/Linux C++ applications.

This repo provides a small set of infrastructure building blocks under `sx::infra`, designed with:
- **Compile firewall**: heavy deps (ZMQ / Asio / JSON / spdlog) are confined to `.cpp` via Pimpl.
- **Thread safety**: safe concurrent read APIs and explicit lifecycle management.
- **Embeddable**: C++17, minimal surface area, `std::error_code`-based error handling where appropriate.

### Key components

- **`sx::infra::InfraManager`**: unified init/shutdown entry point (`init_all()` / `shutdown_all()`).
- **`sx::infra::LogManager` + `sx::infra::ILogger`**: spdlog-backed logging with DI-friendly `ILogger`.
  - Supports **module-level filtering** via logger name (e.g. only enable `"vision"` while disabling others).
- **`sx::infra::AsyncRuntime`**: dual thread-pool runtime (standalone Asio) with IO/CPU isolation.
  - Provides `post_io`, `post_cpu`, `create_timer`, `create_*_strand`, and `spawn_critical_loop`.
- **`sx::infra::ConfigManager`**: JSON config loader with dot-path access (`get<T>(key, default)`).
  - No exceptions required; heavy JSON headers stay in `.cpp`.
- **`sx::infra::UnifiedBus`**: unified messaging bus:
  - Control plane: ZeroMQ PUB/SUB (endpoints as topics) with `std::error_code` return.
  - Data plane: in-process queues for zero-copy `shared_ptr<T>` streams.

### Repository layout

```
common/            # header-only types/utils (sx::types, sx::utils)
infra/             # infrastructure library (sx::infra) + unit tests
modules/           # application modules (placeholder)
cmake/             # CMake helpers and modules (zmq/json/asio/spdlog)
third_party/       # vendored deps (submodules): asio, json, spdlog, zmq, googletest
```

### Dependencies

- **C++17**
- **CMake >= 3.16**
- Vendored via git submodules:
  - standalone **Asio**
  - **nlohmann/json**
  - **spdlog**
  - **libzmq**
  - **googletest**

### Build

Initialize submodules:

```bash
git submodule update --init --recursive --depth 1
```

Configure and build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

Run unit tests:

```bash
cd build
ctest --output-on-failure
```

### Quick start: unified init entrypoint

```cpp
#include "sx/infra/infra_manager.h"

int main() {
    sx::infra::InfraConfig cfg;

    // Logging (optional)
    cfg.enable_logging = true;
    cfg.logging.log_dir = "/tmp";
    cfg.logging.file_name = "app.log";
    cfg.logging.default_level = sx::infra::LogLevel::kInfo;

    // AsyncRuntime pools
    cfg.io_threads = 2;
    cfg.cpu_threads = 0; // auto: hardware_concurrency()
    cfg.scheduler = nullptr; // optional HAL hook

    // Config (optional)
    cfg.config_path = "/etc/app/config.json";

    if (auto ec = sx::infra::InfraManager::init_all(cfg)) {
        // handle init error (e.g. logging/config file errors)
        return 1;
    }

    // ... application ...

    sx::infra::InfraManager::shutdown_all();
    return 0;
}
```

### Using module logging (DI-friendly)

```cpp
#include "sx/infra/logging.h"

auto& lm = sx::infra::LogManager::instance();
(void)lm.init({});

auto vision_log = lm.get_logger("vision"); // inject into module
lm.set_level("vision", sx::infra::LogLevel::kDebug);
lm.set_level("net", sx::infra::LogLevel::kOff); // disable a module

vision_log->debug("vision module debug enabled");
```

### Coding style & notes

- Prefer `std::error_code` for recoverable errors in infra APIs.
- Keep heavy dependencies out of public headers (Pimpl).
- For embedded targets, exceptions are optional; infra code avoids `try/catch` where requested.

