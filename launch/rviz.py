import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    pkg = get_package_share_directory("fm_driver")
    rviz_config = os.path.join(pkg, "rviz", "fm_driver_ros2.rviz")

    port = LaunchConfiguration("port")
    baudrate = LaunchConfiguration("baudrate")
    frame_id = LaunchConfiguration("frame_id")
    parent_frame = LaunchConfiguration("parent_frame")
    static_tf = LaunchConfiguration("static_tf")

    return LaunchDescription(
        [
            DeclareLaunchArgument("port", default_value="/dev/ttyACM0"),
            DeclareLaunchArgument("baudrate", default_value="921600"),
            DeclareLaunchArgument("frame_id", default_value="fm_anchor_link"),
            # 基站坐标系的父坐标系。发布一个静态TF，否则rviz里fm_anchor_link不存在
            DeclareLaunchArgument("parent_frame", default_value="map"),
            # 基站在parent_frame中的安装位姿
            DeclareLaunchArgument("x", default_value="0"),
            DeclareLaunchArgument("y", default_value="0"),
            DeclareLaunchArgument("z", default_value="0"),
            DeclareLaunchArgument("yaw", default_value="0"),
            DeclareLaunchArgument("pitch", default_value="0"),
            DeclareLaunchArgument("roll", default_value="0"),
            # 若已由其它节点发布该TF，置false避免冲突
            DeclareLaunchArgument("static_tf", default_value="true"),
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(
                    os.path.join(pkg, "launch", "msg.py")
                ),
                launch_arguments={
                    "port": port,
                    "baudrate": baudrate,
                    "frame_id": frame_id,
                }.items(),
            ),
            Node(
                package="tf2_ros",
                executable="static_transform_publisher",
                name="fm_anchor_tf",
                condition=IfCondition(static_tf),
                arguments=[
                    "--x", LaunchConfiguration("x"),
                    "--y", LaunchConfiguration("y"),
                    "--z", LaunchConfiguration("z"),
                    "--yaw", LaunchConfiguration("yaw"),
                    "--pitch", LaunchConfiguration("pitch"),
                    "--roll", LaunchConfiguration("roll"),
                    "--frame-id", parent_frame,
                    "--child-frame-id", frame_id,
                ],
            ),
            Node(
                package="rviz2",
                executable="rviz2",
                name="rviz2",
                arguments=["-d", rviz_config],
            ),
        ]
    )
