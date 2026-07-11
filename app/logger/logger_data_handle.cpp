#include "logger_data_handle.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <spdlog/fmt/ranges.h>
#include <spdlog/spdlog.h>
#include <string>

namespace {

const char *connect_type_name(fm_connect_type_e connect_type) {
  switch (connect_type) {
  case FM_WIRED:
    return "WIRED";
  case FM_WIRELESS:
    return "WIRELESS";
  default:
    return "UNKNOWN";
  }
}

const char *role_name(fm_role_e role) {
  switch (role) {
  case FM_ANCHOR:
    return "ANCHOR";
  case FM_TAG:
    return "TAG";
  default:
    return "UNKNOWN";
  }
}

std::string data2hex(const void *data, int size) {
  auto p_data = (const uint8_t *)data;
  return fmt::format("{:02X}", fmt::join(p_data, p_data + size, ""));
}

template <typename T, std::size_t N> std::string vec2str(const T (&value)[N]) {
  return fmt::format("[{}]", fmt::join(value, ","));
}

template <typename T>
const T *get_data(const void *msg_payload, int msg_payload_size) {
  assert(msg_payload_size == int(sizeof(T)));
  return static_cast<const T *>(msg_payload);
}

fm_role_e s_wired_role;
std::string s_frame_suffix;

} // namespace

namespace data_handle {

void on_frame_begin(fm_role_e wired_role, const uint8_t *wired_uid,
                    fm_frame_cnt_t frame_cnt) {
  s_wired_role = wired_role;
  s_frame_suffix =
      fmt::format("{},{}", data2hex(wired_uid, FM_UID_SIZE), frame_cnt);
}
void on_frame_msg(fm_connect_type_e connect_type, fm_msg_id_t msg_id,
                  const void *msg_payload, int msg_payload_size) {
  // 消息来源: FM_WIRED 为有线直连节点，否则为无线对端(角色翻转)
  fm_role_e role = connect_type == FM_WIRED
                       ? s_wired_role
                       : (s_wired_role == FM_ANCHOR ? FM_TAG : FM_ANCHOR);
  auto s_header = fmt::format("{},{},{}", connect_type_name(connect_type),
                              role_name(role), s_frame_suffix);
  switch (msg_id) {
  case FM_MSG_ECHO: {
    auto data = get_data<FMDataEcho>(msg_payload, msg_payload_size);
    spdlog::info("{},MSG_ECHO: {{payload={}}}", s_header,
                 data2hex(data->payload, data->payload_size));
    break;
  }
  case FM_MSG_FIND: {
    auto data = get_data<FMDataFind>(msg_payload, msg_payload_size);
    spdlog::info("{},MSG_FIND: {{duration={}}}", s_header, data->duration);
    break;
  }
  case FM_MSG_RESTART: {
    auto data = get_data<FMDataRestart>(msg_payload, msg_payload_size);
    spdlog::info("{},MSG_RESTART: {{delay={},only_when_need={}}}", s_header,
                 data->delay, data->only_when_need);
    break;
  }
  case FM_MSG_PARAM: {
    auto data = get_data<FMDataParam>(msg_payload, msg_payload_size);
    spdlog::info("{},MSG_PARAM: {{z_expect={},z_expect_noise={},"
                 "max_acceleration={},reserved={},min_dis={},"
                 "min_freq_duration={},min_freq_count={},max_delta_rssi={}}}",
                 s_header, data->z_expect, data->z_expect_noise,
                 vec2str(data->max_acceleration), data->reserved, data->min_dis,
                 data->min_freq_duration, data->min_freq_count,
                 data->max_delta_rssi);
    break;
  }
  case FM_MSG_HEARTBEAT: {
    auto data = get_data<FMDataHeartbeat>(msg_payload, msg_payload_size);
    spdlog::info(
        "{},MSG_HEARTBEAT: {{hardware={{name={},version={},batch={},"
        "date={}-{}-{}}},firmware={{series={},version={}}},need_restart={},"
        "restart_info_dirty={},uptime={}}}",
        s_header, data->hardware.name, vec2str(data->hardware.version),
        data->hardware.batch, data->hardware.year, data->hardware.month,
        data->hardware.day, data->firmware.series,
        vec2str(data->firmware.version), data->need_restart,
        data->restart_info_dirty, data->uptime);
    break;
  }
  case FM_MSG_RESULT: {
    auto data = get_data<FMDataResult>(msg_payload, msg_payload_size);
    spdlog::info("{},MSG_RESULT: {{local_time={},cnt={},pos={},vel={},"
                 "pos_noise={},vel_noise={}}}",
                 s_header, data->local_time, data->cnt, vec2str(data->pos),
                 vec2str(data->vel), vec2str(data->pos_noise),
                 vec2str(data->vel_noise));
    break;
  }
  case FM_MSG_PREV_RESULT: {
    auto data = get_data<FMDataPrevResult>(msg_payload, msg_payload_size);
    spdlog::info("{},MSG_PREV_RESULT: {{cnt={},pos={}}}", s_header, data->cnt,
                 vec2str(data->pos));
    break;
  }
  case FM_MSG_BEGIN_PAIR: {
    auto data = get_data<FMDataBeginPair>(msg_payload, msg_payload_size);
    spdlog::info("{},MSG_BEGIN_PAIR: {{timeout={}}}", s_header, data->timeout);
    break;
  }
  case FM_MSG_CANCEL_PAIR: {
    auto data = get_data<FMDataCancelPair>(msg_payload, msg_payload_size);
    spdlog::info("{},MSG_CANCEL_PAIR: {{timeout={}}}", s_header, data->timeout);
    break;
  }
  case FM_MSG_DATA_USER_TO_USER: {
    auto data = get_data<FMDataDataUserToUser>(msg_payload, msg_payload_size);
    spdlog::info("{},MSG_DATA_USER_TO_USER: {{payload={}}}", s_header,
                 data2hex(data->payload, data->payload_size));
    break;
  }
  case FM_MSG_SPHERICAL_RESULT: {
    auto data = get_data<FMDataSphericalResult>(msg_payload, msg_payload_size);
    spdlog::info("{},MSG_SPHERICAL_RESULT: {{local_time={},cnt={},dis={},"
                 "azimuth={},elevation={}}}",
                 s_header, data->local_time, data->cnt, data->dis,
                 data->azimuth, data->elevation);
    break;
  }
  case FM_MSG_PREV_SPHERICAL_RESULT: {
    auto data =
        get_data<FMDataSphericalPrevResult>(msg_payload, msg_payload_size);
    spdlog::info("{},MSG_PREV_SPHERICAL_RESULT: "
                 "{{cnt={},dis={},azimuth={},elevation={}}}",
                 s_header, data->cnt, data->dis, data->azimuth,
                 data->elevation);
    break;
  }
  case FM_MSG_DIS: {
    auto data = get_data<FMDataDis>(msg_payload, msg_payload_size);
    spdlog::info("{},MSG_DIS: {{local_time={},cnt={},dis={},rx_rate={}}}",
                 s_header, data->local_time, data->cnt, data->dis,
                 data->rx_rate);
    break;
  }
  default: {
    spdlog::info("{},MSG_UNKNOWN({}): {{raw_payload={}}}", s_header, msg_id,
                 data2hex(msg_payload, msg_payload_size));
    break;
  }
  }
}
void on_frame_end() { s_frame_suffix.clear(); }

} // namespace data_handle
