set(TARGET_NAME ros_converter)
add_executable(${TARGET_NAME} main.cpp ros1_data_handle.cpp
                              ros1_data_handle.hpp)
target_link_libraries(${TARGET_NAME} PRIVATE ${LIB_COMMON} ${catkin_LIBRARIES})
target_include_directories(${TARGET_NAME} PRIVATE ${catkin_INCLUDE_DIRS} ..)
add_dependencies(${TARGET_NAME} ${catkin_EXPORTED_TARGETS}
                 ${${PKG_NAME}_EXPORTED_TARGETS})
target_compile_definitions(${TARGET_NAME} PRIVATE APP_NAME="${TARGET_NAME}")
