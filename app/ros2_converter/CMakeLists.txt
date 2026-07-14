set(TARGET_NAME fm_driver_node)
add_executable(${TARGET_NAME} main.cpp ros2_data_handle.cpp
                              ros2_data_handle.hpp)
set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME fm_driver)
target_link_libraries(${TARGET_NAME} ${LIB_COMMON})
ament_target_dependencies(${TARGET_NAME} rclcpp std_msgs geometry_msgs)
rosidl_get_typesupport_target(cpp_typesupport_target ${PKG_NAME}
                              "rosidl_typesupport_cpp")
target_link_libraries(${TARGET_NAME} ${cpp_typesupport_target})
install(TARGETS ${TARGET_NAME} DESTINATION lib/${PKG_NAME})
