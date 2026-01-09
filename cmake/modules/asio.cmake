# usage: include(asio)
# link libraries: asio

if(NOT TARGET asio)
  add_library(asio INTERFACE)

  target_include_directories(asio SYSTEM INTERFACE
    ${PROJECT_SOURCE_DIR}/third_party/asio/include
  )

  # standalone Asio (no Boost dependency)
  target_compile_definitions(asio INTERFACE
    ASIO_STANDALONE
  )
endif()


