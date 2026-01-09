add_sx_library(sx_infra
    src/unified_bus.cpp
    src/config_manager.cpp
    src/async_runtime.cpp
    src/infra_manager.cpp
    src/logging.cpp
)

# 设置包含路径
target_include_directories(sx_infra PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
    $<INSTALL_INTERFACE:include>
)

# 链接依赖
include(zmq)
include(json)
include(asio)
include(spdlog)
find_package(Threads REQUIRED)
target_link_libraries(sx_infra
    PUBLIC
        sx::types
        sx::utils
    PRIVATE
        Threads::Threads
        libzmq
        nlohmann_json::nlohmann_json
        asio
        spdlog::spdlog
)

add_sx_test(sx_infra_test
    ut/unified_bus_test.cpp
    ut/config_manager_test.cpp
    ut/async_runtime_test.cpp
    ut/infra_manager_test.cpp
    ut/logging_test.cpp
)
target_link_libraries(sx_infra_test PRIVATE
    sx_infra
    Threads::Threads
    libzmq
)