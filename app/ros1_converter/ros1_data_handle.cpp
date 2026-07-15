#include "ros1_data_handle.hpp"
#include "main_common.hpp"
#include <algorithm>
#include <cstring>

namespace {
// 单例指针，供无arg的on_frame_msg回调路由
Ros1DataHandle *s_self = nullptr;
} // namespace

Ros1DataHandle::Ros1DataHandle(ros::NodeHandle *nh) : nh_(nh) {
  fm_parser_from_dev_init(&parser_, nullptr, &Ros1DataHandle::on_frame_msg,
                          nullptr);
  s_self = this;

  nh_->param<std::string>("frame_id", frame_id_, "fm_anchor_link");
  ROS_INFO("Frame id: %s", frame_id_.c_str());

  // 设备 -> 用户(^)
  echo_from_device_pub_ =
      nh_->advertise<fm_driver::Echo>("echo_from_device", 50);
  heartbeat_pub_ = nh_->advertise<fm_driver::Heartbeat>("heartbeat", 50);
  param_pub_ = nh_->advertise<fm_driver::Param>("param", 50);
  result_pub_ = nh_->advertise<fm_driver::Result>("result", 50);
  result_pose_pub_ = nh_->advertise<geometry_msgs::PoseWithCovarianceStamped>(
      "result_pose", 50);
  prev_result_pub_ = nh_->advertise<fm_driver::PrevResult>("prev_result", 50);
  user_data_from_device_pub_ =
      nh_->advertise<fm_driver::DataUserToUser>("user_data_from_device", 50);
  spherical_result_pub_ =
      nh_->advertise<fm_driver::SphericalResult>("spherical_result", 50);
  prev_spherical_result_pub_ = nh_->advertise<fm_driver::PrevSphericalResult>(
      "prev_spherical_result", 50);
  dis_pub_ = nh_->advertise<fm_driver::Dis>("dis", 50);

  // 用户 -> 设备(v)
  echo_to_device_sub_ = nh_->subscribe(
      "echo_to_device", 50, &Ros1DataHandle::on_echo_to_device, this);
  find_sub_ = nh_->subscribe("find", 50, &Ros1DataHandle::on_find, this);
  restart_sub_ =
      nh_->subscribe("restart", 50, &Ros1DataHandle::on_restart, this);
  param_read_sub_ =
      nh_->subscribe("param_read", 50, &Ros1DataHandle::on_param_read, this);
  param_write_sub_ =
      nh_->subscribe("param_write", 50, &Ros1DataHandle::on_param_write, this);
  begin_pair_sub_ =
      nh_->subscribe("begin_pair", 50, &Ros1DataHandle::on_begin_pair, this);
  cancel_pair_sub_ =
      nh_->subscribe("cancel_pair", 50, &Ros1DataHandle::on_cancel_pair, this);
  user_data_to_device_sub_ = nh_->subscribe(
      "user_data_to_device", 50, &Ros1DataHandle::on_user_data_to_device, this);
}

Ros1DataHandle::~Ros1DataHandle() { s_self = nullptr; }

void Ros1DataHandle::dispatch(const FMDataEcho &data) {
  fm_driver::Echo msg;
  msg.payload.assign(data.payload, data.payload + data.payload_size);
  echo_from_device_pub_.publish(msg);
}

void Ros1DataHandle::dispatch(const FMDataHeartbeat &data) {
  fm_driver::Heartbeat msg;
  msg.hardware_name.assign(
      data.hardware.name,
      data.hardware.name +
          strnlen(data.hardware.name, sizeof(data.hardware.name)));
  for (size_t i = 0; i < 2; i++) {
    msg.hardware_version[i] = data.hardware.version[i];
  }
  msg.hardware_batch = data.hardware.batch;
  msg.hardware_year = data.hardware.year;
  msg.hardware_month = data.hardware.month;
  msg.hardware_day = data.hardware.day;
  msg.firmware_series = data.firmware.series;
  for (size_t i = 0; i < 4; i++) {
    msg.firmware_version[i] = data.firmware.version[i];
  }
  msg.need_restart = data.need_restart;
  msg.restart_info_dirty = data.restart_info_dirty;
  msg.uptime = data.uptime;
  heartbeat_pub_.publish(msg);
}

void Ros1DataHandle::dispatch(const FMDataParam &data) {
  fm_driver::Param msg;
  msg.z_expect = data.z_expect;
  msg.z_expect_noise = data.z_expect_noise;
  for (size_t i = 0; i < 3; i++) {
    msg.max_acceleration[i] = data.max_acceleration[i];
  }
  msg.reserved = data.reserved;
  msg.min_dis = data.min_dis;
  msg.min_freq_duration = data.min_freq_duration;
  msg.min_freq_count = data.min_freq_count;
  msg.max_delta_rssi = data.max_delta_rssi;
  param_pub_.publish(msg);
}

void Ros1DataHandle::dispatch(const FMDataResult &data) {
  std_msgs::Header header;
  header.stamp = ros::Time::now();
  header.frame_id = frame_id_;
  {
    fm_driver::Result msg;
    msg.header = header;
    msg.local_time = data.local_time;
    msg.cnt = data.cnt;
    for (size_t i = 0; i < 3; i++) {
      msg.pos[i] = data.pos[i];
      msg.vel[i] = data.vel[i];
      msg.pos_noise[i] = data.pos_noise[i];
      msg.vel_noise[i] = data.vel_noise[i];
    }
    result_pub_.publish(msg);
  }
  {
    // 同一份数据再发一份rviz能直接显示的标准消息
    geometry_msgs::PoseWithCovarianceStamped msg;
    msg.header = header;
    msg.pose.pose.position.x = data.pos[0];
    msg.pose.pose.position.y = data.pos[1];
    msg.pose.pose.position.z = data.pos[2];
    msg.pose.pose.orientation.w = 1.0;
    for (size_t i = 0; i < 3; i++) {
      double sigma = data.pos_noise[i];
      msg.pose.covariance[i * 6 + i] = sigma * sigma;
    }
    result_pose_pub_.publish(msg);
  }
}

void Ros1DataHandle::dispatch(const FMDataPrevResult &data) {
  fm_driver::PrevResult msg;
  msg.cnt = data.cnt;
  for (size_t i = 0; i < 3; i++) {
    msg.pos[i] = data.pos[i];
  }
  prev_result_pub_.publish(msg);
}

void Ros1DataHandle::dispatch(const FMDataDataUserToUser &data) {
  fm_driver::DataUserToUser msg;
  msg.payload.assign(data.payload, data.payload + data.payload_size);
  user_data_from_device_pub_.publish(msg);
}

void Ros1DataHandle::dispatch(const FMDataSphericalResult &data) {
  fm_driver::SphericalResult msg;
  msg.local_time = data.local_time;
  msg.cnt = data.cnt;
  msg.dis = data.dis;
  msg.azimuth = data.azimuth;
  msg.elevation = data.elevation;
  spherical_result_pub_.publish(msg);
}

void Ros1DataHandle::dispatch(const FMDataSphericalPrevResult &data) {
  fm_driver::PrevSphericalResult msg;
  msg.cnt = data.cnt;
  msg.dis = data.dis;
  msg.azimuth = data.azimuth;
  msg.elevation = data.elevation;
  prev_spherical_result_pub_.publish(msg);
}

void Ros1DataHandle::dispatch(const FMDataDis &data) {
  fm_driver::Dis msg;
  msg.local_time = data.local_time;
  msg.cnt = data.cnt;
  msg.dis = data.dis;
  msg.rx_rate = data.rx_rate;
  dis_pub_.publish(msg);
}

void Ros1DataHandle::on_echo_to_device(const fm_driver::Echo::ConstPtr &msg) {
  FMDataEcho data{};
  size_t n = std::min(msg->payload.size(), sizeof(data.payload));
  std::copy_n(msg->payload.begin(), n, data.payload);
  data.payload_size = (uint8_t)n;
  main_common_send_msg(FM_WIRED, FM_MSG_ECHO, &data, sizeof(data));
}

void Ros1DataHandle::on_find(const fm_driver::Find::ConstPtr &msg) {
  FMDataFind data{};
  data.duration = msg->duration;
  main_common_send_msg(FM_WIRED, FM_MSG_FIND, &data, sizeof(data));
}

void Ros1DataHandle::on_restart(const fm_driver::Restart::ConstPtr &msg) {
  FMDataRestart data{};
  data.delay = msg->delay;
  data.only_when_need = msg->only_when_need;
  main_common_send_msg(FM_WIRED, FM_MSG_RESTART, &data, sizeof(data));
}

void Ros1DataHandle::on_param_read(const std_msgs::Empty::ConstPtr &msg) {
  (void)msg;
  main_common_send_msg(FM_WIRED, FM_MSG_PARAM_READ, nullptr, 0);
}

void Ros1DataHandle::on_param_write(const fm_driver::Param::ConstPtr &msg) {
  FMDataParam data{};
  data.z_expect = msg->z_expect;
  data.z_expect_noise = msg->z_expect_noise;
  for (size_t i = 0; i < 3; i++) {
    data.max_acceleration[i] = msg->max_acceleration[i];
  }
  data.reserved = msg->reserved;
  data.min_dis = msg->min_dis;
  data.min_freq_duration = msg->min_freq_duration;
  data.min_freq_count = msg->min_freq_count;
  data.max_delta_rssi = msg->max_delta_rssi;
  main_common_send_msg(FM_WIRED, FM_MSG_PARAM_WRITE, &data, sizeof(data));
}

void Ros1DataHandle::on_begin_pair(const fm_driver::BeginPair::ConstPtr &msg) {
  FMDataBeginPair data{};
  data.timeout = msg->timeout;
  main_common_send_msg(FM_WIRED, FM_MSG_BEGIN_PAIR, &data, sizeof(data));
}

void Ros1DataHandle::on_cancel_pair(
    const fm_driver::CancelPair::ConstPtr &msg) {
  FMDataCancelPair data{};
  data.timeout = msg->timeout;
  main_common_send_msg(FM_WIRED, FM_MSG_CANCEL_PAIR, &data, sizeof(data));
}

void Ros1DataHandle::on_user_data_to_device(
    const fm_driver::DataUserToUser::ConstPtr &msg) {
  FMDataDataUserToUser data{};
  size_t n = std::min(msg->payload.size(), sizeof(data.payload));
  std::copy_n(msg->payload.begin(), n, data.payload);
  data.payload_size = (uint8_t)n;
  main_common_send_msg(FM_WIRED, FM_MSG_DATA_USER_TO_USER, &data, sizeof(data));
}

void Ros1DataHandle::on_frame_msg(fm_connect_type_e connect_type,
                                  fm_msg_id_t msg_id, const void *msg_payload,
                                  int msg_payload_size) {
  (void)connect_type;
  (void)msg_payload_size;
  if (!s_self) {
    return;
  }
  switch (msg_id) {
  case FM_MSG_ECHO:
    s_self->dispatch(*static_cast<const FMDataEcho *>(msg_payload));
    break;
  case FM_MSG_HEARTBEAT:
    s_self->dispatch(*static_cast<const FMDataHeartbeat *>(msg_payload));
    break;
  case FM_MSG_PARAM:
    s_self->dispatch(*static_cast<const FMDataParam *>(msg_payload));
    break;
  case FM_MSG_RESULT:
    s_self->dispatch(*static_cast<const FMDataResult *>(msg_payload));
    break;
  case FM_MSG_PREV_RESULT:
    s_self->dispatch(*static_cast<const FMDataPrevResult *>(msg_payload));
    break;
  case FM_MSG_DATA_USER_TO_USER:
    s_self->dispatch(*static_cast<const FMDataDataUserToUser *>(msg_payload));
    break;
  case FM_MSG_SPHERICAL_RESULT:
    s_self->dispatch(*static_cast<const FMDataSphericalResult *>(msg_payload));
    break;
  case FM_MSG_PREV_SPHERICAL_RESULT:
    s_self->dispatch(
        *static_cast<const FMDataSphericalPrevResult *>(msg_payload));
    break;
  case FM_MSG_DIS:
    s_self->dispatch(*static_cast<const FMDataDis *>(msg_payload));
    break;
  default:
    break;
  }
}
