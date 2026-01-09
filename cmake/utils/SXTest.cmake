# 单元配置测试
#  1. 使用本地googletest
#  2. 增加覆盖率测试 (暂时关闭)
#  3. 增加内存泄漏检测 (暂时关闭)
#
# Usage:
#   add_sx_test(test_target test_source.cpp)

include_guard(GLOBAL)

if(NOT BUILD_TESTING)
  return()
endif()

set(GOOGLETEST_DIR ${PROJECT_SOURCE_DIR}/third_party/googletest)

if(NOT EXISTS "${GOOGLETEST_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
      "googletest submodule not initialized.\n"
      "Expected: ${GOOGLETEST_DIR}/CMakeLists.txt\n"
      "Run: git submodule update --init --recursive\n")
endif()

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
option(INSTALL_GMOCK "Install GMock" OFF)
option(INSTALL_GTEST "Install GTest" OFF)

if(NOT TARGET gtest_main)
  add_subdirectory(${GOOGLETEST_DIR} ${CMAKE_BINARY_DIR}/third_party/googletest)
endif()

# Our project uses strict warnings (-Werror). Do not let third-party tests fail due to warnings.
foreach(_t gtest gtest_main gmock gmock_main)
  if(TARGET ${_t})
    target_compile_options(${_t} PRIVATE
      $<$<COMPILE_LANGUAGE:CXX>:-Wno-error>
      $<$<COMPILE_LANGUAGE:CXX>:-Wno-maybe-uninitialized>
    )
  endif()
endforeach()

include(GoogleTest)
# include(Coverage)
# include(Memcheck)

macro(AddTesting target)
  target_link_libraries(${target} PRIVATE gtest_main gmock)
  gtest_discover_tests(${target}
    DISCOVERY_MODE PRE_TEST
    DISCOVERY_TIMEOUT 30
  )
  # AddCoverage(${target})
  # AddMemcheck(${target}) 
endmacro()

# Convenience helper: create a test executable and register it.
function(add_sx_test target)
  add_executable(${target} ${ARGN})
  AddTesting(${target})
endfunction()
