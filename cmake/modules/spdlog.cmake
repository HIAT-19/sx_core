# usage: include(spdlog)
# link libraries: spdlog::spdlog

# Reduce build time; we only need the library.
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(
    ${PROJECT_SOURCE_DIR}/third_party/spdlog
    ${PROJECT_BINARY_DIR}/third_party/spdlog
    EXCLUDE_FROM_ALL)

# Our project uses strict warnings (-Werror). Avoid propagating warnings from
# third-party compilation units/headers as errors.
if(TARGET spdlog)
  target_compile_options(spdlog PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-error>
  )
endif()


