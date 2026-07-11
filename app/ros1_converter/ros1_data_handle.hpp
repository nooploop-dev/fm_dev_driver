#pragma once

#include "fm_dev_driver.h"
#include <fm_dev_driver/BeginPair.h>
#include <fm_dev_driver/CancelPair.h>
#include <fm_dev_driver/DataUserToUser.h>
#include <fm_dev_driver/Dis.h>
#include <fm_dev_driver/Echo.h>
#include <fm_dev_driver/Find.h>
#include <fm_dev_driver/Heartbeat.h>
#include <fm_dev_driver/Param.h>
#include <fm_dev_driver/PrevResult.h>
#include <fm_dev_driver/PrevSphericalResult.h>
#include <fm_dev_driver/Restart.h>
#include <fm_dev_driver/Result.h>
#include <fm_dev_driver/SphericalResult.h>
#include <ros/ros.h>
#include <std_msgs/Empty.h>

class Ros1DataHandle {
public:
  explicit Ros1DataHandle(ros::NodeHandle *nh);
  ~Ros1DataHandle();

  FMParserFromDev *parser() { return &parser_; }

private:
  // 收到设备消息的回调(C接口，无arg，路由到单例)
  static void on_frame_msg(bool wired, fm_msg_id_t msg_id,
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
  void on_echo_to_device(const fm_dev_driver::Echo::ConstPtr &msg);
  void on_find(const fm_dev_driver::Find::ConstPtr &msg);
  void on_restart(const fm_dev_driver::Restart::ConstPtr &msg);
  void on_param_read(const std_msgs::Empty::ConstPtr &msg);
  void on_param_write(const fm_dev_driver::Param::ConstPtr &msg);
  void on_begin_pair(const fm_dev_driver::BeginPair::ConstPtr &msg);
  void on_cancel_pair(const fm_dev_driver::CancelPair::ConstPtr &msg);
  void
  on_user_data_to_device(const fm_dev_driver::DataUserToUser::ConstPtr &msg);

  FMParserFromDev parser_;
  ros::NodeHandle *nh_;

  ros::Publisher echo_from_device_pub_;
  ros::Publisher heartbeat_pub_;
  ros::Publisher param_pub_;
  ros::Publisher result_pub_;
  ros::Publisher prev_result_pub_;
  ros::Publisher user_data_from_device_pub_;
  ros::Publisher spherical_result_pub_;
  ros::Publisher prev_spherical_result_pub_;
  ros::Publisher dis_pub_;

  ros::Subscriber echo_to_device_sub_;
  ros::Subscriber find_sub_;
  ros::Subscriber restart_sub_;
  ros::Subscriber param_read_sub_;
  ros::Subscriber param_write_sub_;
  ros::Subscriber begin_pair_sub_;
  ros::Subscriber cancel_pair_sub_;
  ros::Subscriber user_data_to_device_sub_;
};
