#pragma once

#include "fm_driver_for_user.h"
#include <fm_driver/msg/begin_pair.hpp>
#include <fm_driver/msg/cancel_pair.hpp>
#include <fm_driver/msg/data_user_to_user.hpp>
#include <fm_driver/msg/dis.hpp>
#include <fm_driver/msg/echo.hpp>
#include <fm_driver/msg/find.hpp>
#include <fm_driver/msg/heartbeat.hpp>
#include <fm_driver/msg/param.hpp>
#include <fm_driver/msg/prev_result.hpp>
#include <fm_driver/msg/prev_spherical_result.hpp>
#include <fm_driver/msg/restart.hpp>
#include <fm_driver/msg/result.hpp>
#include <fm_driver/msg/spherical_result.hpp>
#include <geometry_msgs/msg/pose_with_covariance_stamped.hpp>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/empty.hpp>

class Ros2DataHandle {
public:
  explicit Ros2DataHandle(const rclcpp::Node::SharedPtr &node);
  ~Ros2DataHandle();

  FMParserFromDev *parser() { return &parser_; }

private:
  // 收到设备消息的回调(C接口，无arg，路由到单例)
  static void on_frame_msg(fm_connect_type_e connect_type, fm_msg_id_t msg_id,
                           const void *msg_payload, int msg_payload_size);
  // 设备 -> 用户(^)，解析后发布到话题
  void dispatch(const FMDataEcho &data);
  void dispatch(const FMDataHeartbeat &data);
  void dispatch(const FMDataParam &data);
  void dispatch(const FMDataResult &data);
  void dispatch(const FMDataPrevResult &data);
  void dispatch(const FMDataDataUserToUser &data);
  void dispatch(const FMDataSphericalResult &data);
  void dispatch(const FMDataSphericalPrevResult &data);
  void dispatch(const FMDataDis &data);

  // 用户 -> 设备(v)，订阅话题后发送给设备
  void on_echo_to_device(const fm_driver::msg::Echo::SharedPtr msg);
  void on_find(const fm_driver::msg::Find::SharedPtr msg);
  void on_restart(const fm_driver::msg::Restart::SharedPtr msg);
  void on_param_read(const std_msgs::msg::Empty::SharedPtr msg);
  void on_param_write(const fm_driver::msg::Param::SharedPtr msg);
  void on_begin_pair(const fm_driver::msg::BeginPair::SharedPtr msg);
  void on_cancel_pair(const fm_driver::msg::CancelPair::SharedPtr msg);
  void
  on_user_data_to_device(const fm_driver::msg::DataUserToUser::SharedPtr msg);

  FMParserFromDev parser_;
  rclcpp::Node::SharedPtr node_;
  std::string frame_id_;

  rclcpp::Publisher<fm_driver::msg::Echo>::SharedPtr echo_from_device_pub_;
  rclcpp::Publisher<fm_driver::msg::Heartbeat>::SharedPtr heartbeat_pub_;
  rclcpp::Publisher<fm_driver::msg::Param>::SharedPtr param_pub_;
  rclcpp::Publisher<fm_driver::msg::Result>::SharedPtr result_pub_;
  rclcpp::Publisher<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr
      result_pose_pub_;
  rclcpp::Publisher<fm_driver::msg::PrevResult>::SharedPtr prev_result_pub_;
  rclcpp::Publisher<fm_driver::msg::DataUserToUser>::SharedPtr
      user_data_from_device_pub_;
  rclcpp::Publisher<fm_driver::msg::SphericalResult>::SharedPtr
      spherical_result_pub_;
  rclcpp::Publisher<fm_driver::msg::PrevSphericalResult>::SharedPtr
      prev_spherical_result_pub_;
  rclcpp::Publisher<fm_driver::msg::Dis>::SharedPtr dis_pub_;

  rclcpp::Subscription<fm_driver::msg::Echo>::SharedPtr echo_to_device_sub_;
  rclcpp::Subscription<fm_driver::msg::Find>::SharedPtr find_sub_;
  rclcpp::Subscription<fm_driver::msg::Restart>::SharedPtr restart_sub_;
  rclcpp::Subscription<std_msgs::msg::Empty>::SharedPtr param_read_sub_;
  rclcpp::Subscription<fm_driver::msg::Param>::SharedPtr param_write_sub_;
  rclcpp::Subscription<fm_driver::msg::BeginPair>::SharedPtr begin_pair_sub_;
  rclcpp::Subscription<fm_driver::msg::CancelPair>::SharedPtr cancel_pair_sub_;
  rclcpp::Subscription<fm_driver::msg::DataUserToUser>::SharedPtr
      user_data_to_device_sub_;
};
