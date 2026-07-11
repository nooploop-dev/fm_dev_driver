# 黑盒测试: 仅依赖对外的 fm_driver.h
set(TARGET_NAME test_fm_driver)
add_executable(${TARGET_NAME} ${TARGET_NAME}.cpp)
target_link_libraries(${TARGET_NAME} PRIVATE ${LIB_DEV_DRIVER}
                                             Catch2::Catch2WithMain)
add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})

# 白盒测试: 验证内部 wire 格式行为(如 payload 版本兼容)，需访问内部头(src/)
set(INTERNAL_TARGET test_fm_driver_internal)
add_executable(${INTERNAL_TARGET} test_version_compat.cpp)
target_link_libraries(${INTERNAL_TARGET} PRIVATE ${LIB_DEV_DRIVER}
                                                 Catch2::Catch2WithMain)
target_include_directories(${INTERNAL_TARGET}
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
add_test(NAME ${INTERNAL_TARGET} COMMAND ${INTERNAL_TARGET})
