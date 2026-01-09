# usage: include(zmq)
# link libraries: libzmq

# 禁用 libzmq 的测试和额外工具以加快构建
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WITH_PERF_TOOL OFF CACHE BOOL "" FORCE)
set(ZMQ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ENABLE_DRAFTS OFF CACHE BOOL "" FORCE)
set(WITH_DOCS OFF CACHE BOOL "" FORCE)

add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/zmq ${PROJECT_BINARY_DIR}/third_party/zmq EXCLUDE_FROM_ALL)
