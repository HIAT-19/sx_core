# =========================================================
# sx_types (Header-only)
# =========================================================
add_sx_library(sx_types INTERFACE)

target_include_directories(sx_types INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
    $<INSTALL_INTERFACE:include>
)

# =========================================================
# sx_utils (Header-only)
# =========================================================
add_sx_library(sx_utils INTERFACE)

target_include_directories(sx_utils INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
    $<INSTALL_INTERFACE:include>
)

if(BUILD_TESTING)
    # add_subdirectory(tests)
endif()
