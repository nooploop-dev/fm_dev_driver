#include "ros2_data_handle.hpp"
#include "main_common.hpp"
#include <algorithm>
#include <cstring>

namespace {
// 单例指针，供无arg的on_frame_msg回调路由
Ros2DataHandle *s_self = nullptr;
} // namespace

Ros2DataHandle::Ros2DataHandle(const rclcpp::Node::SharedPtr &node)
    : node_(node) {
  fm_parser_from_dev_init(&parser_, nullptr, &Ros2DataHandle::on_frame_msg,
                          nullptr);
  s_self = this;

  // 设备 -> 用户(^)
  echo_from_device_pub_ =
      node_->create_publisher<fm_driver::msg::Echo>("~/echo_from_device", 50);
  heartbeat_pub_ =
      node_->create_publisher<fm_driver::msg::Heartbeat>("~/heartbeat", 50);
  param_pub_ = node_->create_publisher<fm_driver::msg::Param>("~/param", 50);
  result_pub_ = node_->create_publisher<fm_driver::msg::Result>("~/result", 50);
  prev_result_pub_ =
      node_->create_publisher<fm_driver::msg::PrevResult>("~/prev_result", 50);
  user_data_from_device_pub_ =
      node_->create_publisher<fm_driver::msg::DataUserToUser>(
          "~/user_data_from_device", 50);
  spherical_result_pub_ =
      node_->create_publisher<fm_driver::msg::SphericalResult>(
          "~/spherical_result", 50);
  prev_spherical_result_pub_ =
      node_->create_publisher<fm_driver::msg::PrevSphericalResult>(
          "~/prev_spherical_result", 50);
  dis_pub_ = node_->create_publisher<fm_driver::msg::Dis>("~/dis", 50);

  // 用户 -> 设备(v)
  echo_to_device_sub_ = node_->create_subscription<fm_driver::msg::Echo>(
      "~/echo_to_device", 50,
      [this](const fm_driver::msg::Echo::SharedPtr msg) {
        on_echo_to_device(msg);
      });
  find_sub_ = node_->create_subscription<fm_driver::msg::Find>(
      "~/find", 50,
      [this](const fm_driver::msg::Find::SharedPtr msg) { on_find(msg); });
  restart_sub_ = node_->create_subscription<fm_driver::msg::Restart>(
      "~/restart", 50, [this](const fm_driver::msg::Restart::SharedPtr msg) {
        on_restart(msg);
      });
  param_read_sub_ = node_->create_subscription<std_msgs::msg::Empty>(
      "~/param_read", 50, [this](const std_msgs::msg::Empty::SharedPtr msg) {
        on_param_read(msg);
      });
  param_write_sub_ = node_->create_subscription<fm_driver::msg::Param>(
      "~/param_write", 50, [this](const fm_driver::msg::Param::SharedPtr msg) {
        on_param_write(msg);
      });
  begin_pair_sub_ = node_->create_subscription<fm_driver::msg::BeginPair>(
      "~/begin_pair", 50,
      [this](const fm_driver::msg::BeginPair::SharedPtr msg) {
        on_begin_pair(msg);
      });
  cancel_pair_sub_ = node_->create_subscription<fm_driver::msg::CancelPair>(
      "~/cancel_pair", 50,
      [this](const fm_driver::msg::CancelPair::SharedPtr msg) {
        on_cancel_pair(msg);
      });
  user_data_to_device_sub_ =
      node_->create_subscription<fm_driver::msg::DataUserToUser>(
          "~/user_data_to_device", 50,
          [this](const fm_driver::msg::DataUserToUser::SharedPtr msg) {
            on_user_data_to_device(msg);
          });
}

Ros2DataHandle::~Ros2DataHandle() { s_self = nullptr; }

void Ros2DataHandle::dispatch(const FMDataEcho &data) {
  fm_driver::msg::Echo msg;
  msg.payload.assign(data.payload, data.payload + data.payload_size);
  echo_from_device_pub_->publish(msg);
}

void Ros2DataHandle::dispatch(const FMDataHeartbeat &data) {
  fm_driver::msg::Heartbeat msg;
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
  heartbeat_pub_->publish(msg);
}

void Ros2DataHandle::dispatch(const FMDataParam &data) {
  fm_driver::msg::Param msg;
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
  param_pub_->publish(msg);
}

void Ros2DataHandle::dispatch(const FMDataResult &data) {
  fm_driver::msg::Result msg;
  msg.local_time = data.local_time;
  msg.cnt = data.cnt;
  for (size_t i = 0; i < 3; i++) {
    msg.pos[i] = data.pos[i];
    msg.vel[i] = data.vel[i];
    msg.pos_noise[i] = data.pos_noise[i];
    msg.vel_noise[i] = data.vel_noise[i];
  }
  result_pub_->publish(msg);
}

void Ros2DataHandle::dispatch(const FMDataPrevResult &data) {
  fm_driver::msg::PrevResult msg;
  msg.cnt = data.cnt;
  for (size_t i = 0; i < 3; i++) {
    msg.pos[i] = data.pos[i];
  }
  prev_result_pub_->publish(msg);
}

void Ros2DataHandle::dispatch(const FMDataDataUserToUser &data) {
  fm_driver::msg::DataUserToUser msg;
  msg.payload.assign(data.payload, data.payload + data.payload_size);
  user_data_from_device_pub_->publish(msg);
}

void Ros2DataHandle::dispatch(const FMDataSphericalResult &data) {
  fm_driver::msg::SphericalResult msg;
  msg.local_time = data.local_time;
  msg.cnt = data.cnt;
  msg.dis = data.dis;
  msg.azimuth = data.azimuth;
  msg.elevation = data.elevation;
  spherical_result_pub_->publish(msg);
}

void Ros2DataHandle::dispatch(const FMDataSphericalPrevResult &data) {
  fm_driver::msg::PrevSphericalResult msg;
  msg.cnt = data.cnt;
  msg.dis = data.dis;
  msg.azimuth = data.azimuth;
  msg.elevation = data.elevation;
  prev_spherical_result_pub_->publish(msg);
}

void Ros2DataHandle::dispatch(const FMDataDis &data) {
  fm_driver::msg::Dis msg;
  msg.local_time = data.local_time;
  msg.cnt = data.cnt;
  msg.dis = data.dis;
  msg.rx_rate = data.rx_rate;
  dis_pub_->publish(msg);
}

void Ros2DataHandle::on_echo_to_device(
    const fm_driver::msg::Echo::SharedPtr msg) {
  FMDataEcho data{};
  size_t n = std::min(msg->payload.size(), sizeof(data.payload));
  std::copy_n(msg->payload.begin(), n, data.payload);
  data.payload_size = (uint8_t)n;
  main_common_send_msg(FM_WIRED, FM_MSG_ECHO, &data, sizeof(data));
}

void Ros2DataHandle::on_find(const fm_driver::msg::Find::SharedPtr msg) {
  FMDataFind data{};
  data.duration = msg->duration;
  main_common_send_msg(FM_WIRED, FM_MSG_FIND, &data, sizeof(data));
}

void Ros2DataHandle::on_restart(const fm_driver::msg::Restart::SharedPtr msg) {
  FMDataRestart data{};
  data.delay = msg->delay;
  data.only_when_need = msg->only_when_need;
  main_common_send_msg(FM_WIRED, FM_MSG_RESTART, &data, sizeof(data));
}

void Ros2DataHandle::on_param_read(const std_msgs::msg::Empty::SharedPtr msg) {
  (void)msg;
  main_common_send_msg(FM_WIRED, FM_MSG_PARAM_READ, nullptr, 0);
}

void Ros2DataHandle::on_param_write(
    const fm_driver::msg::Param::SharedPtr msg) {
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

void Ros2DataHandle::on_begin_pair(
    const fm_driver::msg::BeginPair::SharedPtr msg) {
  FMDataBeginPair data{};
  data.timeout = msg->timeout;
  main_common_send_msg(FM_WIRED, FM_MSG_BEGIN_PAIR, &data, sizeof(data));
}

void Ros2DataHandle::on_cancel_pair(
    const fm_driver::msg::CancelPair::SharedPtr msg) {
  FMDataCancelPair data{};
  data.timeout = msg->timeout;
  main_common_send_msg(FM_WIRED, FM_MSG_CANCEL_PAIR, &data, sizeof(data));
}

void Ros2DataHandle::on_user_data_to_device(
    const fm_driver::msg::DataUserToUser::SharedPtr msg) {
  FMDataDataUserToUser data{};
  size_t n = std::min(msg->payload.size(), sizeof(data.payload));
  std::copy_n(msg->payload.begin(), n, data.payload);
  data.payload_size = (uint8_t)n;
  main_common_send_msg(FM_WIRED, FM_MSG_DATA_USER_TO_USER, &data, sizeof(data));
}

void Ros2DataHandle::on_frame_msg(fm_connect_type_e connect_type,
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
