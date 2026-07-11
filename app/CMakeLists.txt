FetchContent_Declare(
  asio
  GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
  GIT_TAG asio-1-34-2)
FetchContent_MakeAvailable(asio)
add_library(asio INTERFACE)
target_include_directories(asio INTERFACE ${asio_SOURCE_DIR}/asio/include)

set(LIB_COMMON ${PKG_NAME}_common)
add_library(${LIB_COMMON} STATIC serial_port.cpp serial_port.hpp
                                 main_common.cpp main_common.hpp)
target_include_directories(${LIB_COMMON} PUBLIC .)
target_link_libraries(${LIB_COMMON} PUBLIC ${LIB_DEV_DRIVER} asio)

if(FM_BUILD_READER OR FM_BUILD_WRITER)
  if(NOT TARGET spdlog)
    FetchContent_Declare(
      spdlog
      GIT_REPOSITORY https://github.com/gabime/spdlog.git
      GIT_TAG v1.15.3)
    FetchContent_MakeAvailable(spdlog)
  endif()
endif()

if(FM_BUILD_READER)
  message(STATUS "build reader")
  add_subdirectory(reader)
endif()

if(FM_BUILD_WRITER)
  message(STATUS "build writer")
  add_subdirectory(writer)
endif()

if(FM_BUILD_ROS1)
  message(STATUS "build ROS1 Converter")
  add_subdirectory(ros1_converter)
endif()

if(FM_BUILD_ROS2)
  message(STATUS "build ROS2 Converter")
  add_subdirectory(ros2_converter)
endif()
