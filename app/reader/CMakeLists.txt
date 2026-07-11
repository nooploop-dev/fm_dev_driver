FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.15.3)
FetchContent_MakeAvailable(spdlog)

set(TARGET_NAME reader)
add_executable(${TARGET_NAME} main.cpp reader_data_handle.cpp
                              reader_data_handle.hpp)
target_link_libraries(${TARGET_NAME} PRIVATE ${LIB_COMMON} spdlog::spdlog)
target_include_directories(${TARGET_NAME} PRIVATE ..)
target_compile_definitions(${TARGET_NAME} PRIVATE APP_NAME="${TARGET_NAME}")
