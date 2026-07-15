set(TARGET_NAME fm_driver_node)
add_executable(
    ${TARGET_NAME}
    main.cpp
    ros1_data_handle.cpp
    ros1_data_handle.hpp
)
set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME fm_driver)
target_link_libraries(${TARGET_NAME} PRIVATE ${LIB_COMMON} ${catkin_LIBRARIES})
target_include_directories(${TARGET_NAME} PRIVATE ${catkin_INCLUDE_DIRS} ..)
add_dependencies(
    ${TARGET_NAME}
    ${catkin_EXPORTED_TARGETS}
    ${${PKG_NAME}_EXPORTED_TARGETS}
)
