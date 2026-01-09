cmake_minimum_required(VERSION 3.16)
project(sx_core C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate compile commands for tools like clang-tidy and clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set the cmake script directory
list(APPEND CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
list(APPEND CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/utils/")
list(APPEND CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/")

# Forbid in-source builds
include(NoInSourceBuilds)

# Using BUILD_TESTING control whether the CTest module invokes enable_testing().
include(CTest)

# Include custom library function
include(SXLibrary)

# Include custom test framework
include(SXTest)

# Set output directories for binaries and libraries
include(SetOutputDirectories)

add_subdirectory(common)
add_subdirectory(infra)
add_subdirectory(modules)
