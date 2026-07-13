cmake_minimum_required(VERSION 3.22)

set(PKG_NAME fm_driver)

project(${PKG_NAME})

option(FM_BUILD_READER "build reader" ON)
option(FM_BUILD_WRITER "build writer" ON)
option(FM_BUILD_ROS1 "build ROS1 converter" OFF)
option(FM_BUILD_ROS2 "build ROS2 converter" OFF)
option(FM_BUILD_TEST "build test" ON)

# 未显式选择ROS版本时，尝试根据环境变量自动检测
if(NOT FM_BUILD_ROS1 AND NOT FM_BUILD_ROS2)
  if(DEFINED ENV{ROS_VERSION})
    if("$ENV{ROS_VERSION}" STREQUAL "1")
      set(FM_BUILD_ROS1 ON)
    elseif("$ENV{ROS_VERSION}" STREQUAL "2")
      set(FM_BUILD_ROS2 ON)
    endif()
  endif()
endif()

# 非ROS构建时默认启用FOXGLOVE可视化支持
if(FM_BUILD_ROS1 OR FM_BUILD_ROS2)
  set(FM_FOXGLOVE_DEFAULT OFF)
else()
  set(FM_FOXGLOVE_DEFAULT ON)
endif()
option(FM_BUILD_FOXGLOVE "build reader with foxglove visualization"
       ${FM_FOXGLOVE_DEFAULT})

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_definitions("$<$<CONFIG:Debug>:DEBUG>")
foreach(lang C CXX)
  foreach(config RELEASE MINSIZEREL RELWITHDEBINFO)
    string(REGEX REPLACE "[-/]D[ ]?NDEBUG" "" CMAKE_${lang}_FLAGS_${config}
                         "${CMAKE_${lang}_FLAGS_${config}}")
  endforeach()
endforeach()
if(MSVC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /utf-8")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
  add_compile_options(/W4)
  add_compile_options(/permissive-)
  add_compile_definitions(NOMINMAX)
  add_compile_definitions(_WIN32_WINNT=0x0A00)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(LIB_DEV_DRIVER lib_${PKG_NAME})
add_library(${LIB_DEV_DRIVER} STATIC src/fm_crc.c src/fm_frame.c
                                     src/fm_driver.c)
target_include_directories(${LIB_DEV_DRIVER} PUBLIC include)

include(FetchContent)
if(FM_BUILD_TEST)
  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1)
  FetchContent_MakeAvailable(Catch2)
  enable_testing()
  add_subdirectory(test)
endif()

if(FM_BUILD_ROS1)
  find_package(catkin REQUIRED COMPONENTS roscpp std_msgs geometry_msgs
                                          message_generation)
  add_message_files(
    FILES
    Result.msg
    SphericalResult.msg
    DataUserToUser.msg
    Echo.msg
    Find.msg
    Restart.msg
    Param.msg
    Heartbeat.msg
    PrevResult.msg
    BeginPair.msg
    CancelPair.msg
    PrevSphericalResult.msg
    Dis.msg)
  generate_messages(DEPENDENCIES std_msgs)
  catkin_package(CATKIN_DEPENDS roscpp std_msgs geometry_msgs message_runtime)
endif()

if(FM_BUILD_ROS2)
  find_package(ament_cmake REQUIRED)
  find_package(rclcpp REQUIRED)
  find_package(std_msgs REQUIRED)
  find_package(geometry_msgs REQUIRED)
  find_package(rosidl_default_generators REQUIRED)
  rosidl_generate_interfaces(
    ${PKG_NAME}
    "msg/Result.msg"
    "msg/SphericalResult.msg"
    "msg/DataUserToUser.msg"
    "msg/Echo.msg"
    "msg/Find.msg"
    "msg/Restart.msg"
    "msg/Param.msg"
    "msg/Heartbeat.msg"
    "msg/PrevResult.msg"
    "msg/BeginPair.msg"
    "msg/CancelPair.msg"
    "msg/PrevSphericalResult.msg"
    "msg/Dis.msg"
    DEPENDENCIES
    std_msgs)
  ament_export_dependencies(rosidl_default_runtime)
  install(DIRECTORY launch rviz DESTINATION share/${PKG_NAME})
endif()

add_subdirectory(app)

if(FM_BUILD_ROS2)
  ament_package()
endif()
