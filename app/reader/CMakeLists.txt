set(TARGET_NAME reader)
add_executable(${TARGET_NAME} main.cpp reader_data_handle.cpp
                              reader_data_handle.hpp foxglove_viz.hpp)
target_link_libraries(${TARGET_NAME} PRIVATE ${LIB_COMMON} spdlog_local
                                             CLI11::CLI11)
target_include_directories(${TARGET_NAME} PRIVATE ..)
target_compile_definitions(${TARGET_NAME} PRIVATE APP_NAME="${TARGET_NAME}")

if(FM_BUILD_FOXGLOVE)
  # 官方提供各平台预编译包(内含Rust核心库+C++封装源码)，无需Rust工具链
  set(FOXGLOVE_VERSION 0.25.3)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    set(_fg_arch aarch64)
  else()
    set(_fg_arch x86_64)
  endif()
  if(WIN32)
    set(_fg_target ${_fg_arch}-pc-windows-msvc)
  elseif(APPLE)
    set(_fg_target ${_fg_arch}-apple-darwin)
  else()
    set(_fg_target ${_fg_arch}-unknown-linux-gnu)
  endif()

  FetchContent_Declare(
    foxglove_sdk
    URL https://github.com/foxglove/foxglove-sdk/releases/download/sdk/v${FOXGLOVE_VERSION}/foxglove-v${FOXGLOVE_VERSION}-cpp-${_fg_target}.zip
  )
  FetchContent_MakeAvailable(foxglove_sdk)

  find_package(foxglove-sdk CONFIG REQUIRED HINTS ${foxglove_sdk_SOURCE_DIR})
  foxglove_sdk_add_cpp_library(foxglove_cpp TYPE STATIC)

  target_sources(${TARGET_NAME} PRIVATE foxglove_viz.cpp)
  target_link_libraries(${TARGET_NAME} PRIVATE foxglove_cpp)
else()
  # 未启用时链接空实现，reader不引入任何Foxglove代码
  target_sources(${TARGET_NAME} PRIVATE foxglove_viz_stub.cpp)
endif()
