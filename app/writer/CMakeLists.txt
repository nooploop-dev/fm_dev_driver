set(TARGET_NAME writer)
add_executable(${TARGET_NAME} main.cpp)
target_link_libraries(${TARGET_NAME} PRIVATE ${LIB_COMMON} spdlog::spdlog
                                             CLI11::CLI11)
target_include_directories(${TARGET_NAME} PRIVATE ..)
target_compile_definitions(${TARGET_NAME} PRIVATE APP_NAME="${TARGET_NAME}")
