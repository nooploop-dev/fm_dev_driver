#include "main_common.hpp"
#include "ros2_data_handle.hpp"
#include <rclcpp/logging.hpp>
#include <rclcpp/rclcpp.hpp>
#include <thread>

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);

  auto node = std::make_shared<rclcpp::Node>("fm_driver");
  node->declare_parameter<std::string>("port", "");
  node->declare_parameter<int>("baudrate", 0);
  auto port = node->get_parameter("port").as_string();
  auto baudrate = int(node->get_parameter("baudrate").as_int());
  RCLCPP_INFO(node->get_logger(), "Serial port: %s", port.c_str());
  RCLCPP_INFO(node->get_logger(), "Baud rate: %d", baudrate);

  Ros2DataHandle data_handle(node);

  std::thread spin_thread([&node]() { rclcpp::spin(node); });
  main_common_init(port, baudrate, data_handle.parser());
  rclcpp::shutdown();
  if (spin_thread.joinable()) {
    spin_thread.join();
  }
  return 0;
}
