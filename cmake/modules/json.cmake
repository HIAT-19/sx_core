# usage: include(json)
# link libraries: nlohmann_json::nlohmann_json

# Disable nlohmann/json tests and install to speed up build.
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)

add_subdirectory(
    ${PROJECT_SOURCE_DIR}/third_party/json
    ${PROJECT_BINARY_DIR}/third_party/json
    EXCLUDE_FROM_ALL)


