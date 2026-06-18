#include "main_common.hpp"
#include "ros1_data_handle.hpp"
#include <ros/ros.h>
#include <thread>

int main(int argc, char **argv) {
  ros::init(argc, argv, "ros_converter");
  ros::NodeHandle nh("~");

  std::string log_dir;
  std::string log_level;
  std::string port;
  int baudrate = 0;

  nh.param<std::string>("port", port, port);
  nh.param<int>("baudrate", baudrate, baudrate);

  Ros1DataHandle data_handle(&nh);
  std::thread spin_thread([]() { ros::spin(); });
  main_common_init(port, baudrate, data_handle.parser());
  ros::shutdown();
  if (spin_thread.joinable()) {
    spin_thread.join();
  }
  return 0;
}
