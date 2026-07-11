#include "catch2/catch_all.hpp"
#include "fm_driver.h"
#include "fm_driver_for_dev.h"
// 白盒测试: 需要构造 payload 大小与"当前版本结构体"不一致的帧，
// 这只能在内部 wire 层手工拼装(对外接口总是产生正确大小)，故依赖内部头。
#include "fm_crc.h"
#include "fm_driver_raw.h"
#include "fm_frame.h"
#include "fm_msg.h"
#include <cstring>
#include <vector>

namespace {

struct Captured {
  fm_msg_id_t id;
  std::vector<uint8_t> payload;
};
std::vector<Captured> g_msgs;

void on_from_user_msg(fm_connect_type_e connect_type, fm_msg_id_t id,
                      const void *payload, int size) {
  (void)connect_type;
  Captured c{};
  c.id = id;
  const uint8_t *p = static_cast<const uint8_t *>(payload);
  if (p && size > 0) {
    c.payload.assign(p, p + size);
  }
  g_msgs.push_back(c);
}

// 构造一帧 user->dev，含单个 msg(id + 指定字节数的 payload)。
// payload_size 可故意不等于"当前版本"协议结构体大小，用于验证版本兼容处理。
std::vector<uint8_t> craft_frame(fm_frame_cnt_t cnt, fm_msg_id_t id,
                                 const uint8_t *payload, int payload_size) {
  FMFrameUserToDev frame;
  std::memset(&frame, 0, sizeof(frame));
  frame.sof = FM_FRAME_SOF;
  frame.cnt = cnt;
  frame.payload_size = (fm_frame_payload_size_t)fm_msg_write(
      frame.payload, id, payload, payload_size);
  int n = fm_frame_user_to_dev_bytes(&frame);
  fm_crc16_update(&frame, n);
  return std::vector<uint8_t>(reinterpret_cast<const uint8_t *>(&frame),
                              reinterpret_cast<const uint8_t *>(&frame) + n);
}

} // namespace

TEST_CASE("msg payload version compatibility") {
  g_msgs.clear();
  FMParserFromUser parser;
  fm_parser_from_user_init(&parser, nullptr, on_from_user_msg, nullptr);

  // 取一个含定长整数末尾字段的协议结构体，便于观察末尾字段
  FMRawDataParam raw{};
  raw.z_expect = 1.0f;
  raw.z_expect_noise = 2.0f;
  raw.max_acceleration[0] = 3.0f;
  raw.max_acceleration[1] = 4.0f;
  raw.max_acceleration[2] = 5.0f;
  raw.min_dis = 6.0f;
  raw.min_freq_duration = 7.0f;
  raw.min_freq_count = 9;
  raw.max_delta_rssi = 0xAB; // 末尾字段
  const uint8_t *raw_bytes = reinterpret_cast<const uint8_t *>(&raw);
  const int raw_size = static_cast<int>(sizeof(raw));

  SECTION("payload larger than struct: extra trailing bytes are discarded") {
    // 模拟"新版"设备在末尾追加了4个字节
    std::vector<uint8_t> wire(raw_bytes, raw_bytes + raw_size);
    wire.insert(wire.end(), {0xDE, 0xAD, 0xBE, 0xEF});
    auto frame = craft_frame(1, FM_MSG_PARAM, wire.data(), (int)wire.size());

    fm_parser_from_user_handle_data(&parser, frame.data(), (int)frame.size());

    REQUIRE(g_msgs.size() == 1);
    REQUIRE(g_msgs[0].id == FM_MSG_PARAM);
    // 多出的字节被舍弃，结构体不受影响，所有字段保持原值
    REQUIRE(g_msgs[0].payload.size() == sizeof(FMDataParam));
    const FMDataParam *p =
        reinterpret_cast<const FMDataParam *>(g_msgs[0].payload.data());
    REQUIRE(p->z_expect == 1.0f);
    REQUIRE(p->min_freq_duration == 7.0f);
    REQUIRE(p->min_freq_count == 9);
    REQUIRE(p->max_delta_rssi == 0xAB);
  }

  SECTION(
      "payload smaller than struct: missing trailing field is zero-filled") {
    // 模拟"旧版"设备缺少末尾的 max_delta_rssi(1字节)
    const int short_size =
        raw_size - static_cast<int>(sizeof(raw.max_delta_rssi));
    auto frame = craft_frame(2, FM_MSG_PARAM, raw_bytes, short_size);

    fm_parser_from_user_handle_data(&parser, frame.data(), (int)frame.size());

    REQUIRE(g_msgs.size() == 1);
    REQUIRE(g_msgs[0].id == FM_MSG_PARAM);
    const FMDataParam *p =
        reinterpret_cast<const FMDataParam *>(g_msgs[0].payload.data());
    // 已发送的字段保留原值
    REQUIRE(p->z_expect == 1.0f);
    REQUIRE(p->min_freq_duration == 7.0f);
    REQUIRE(p->min_freq_count == 9);
    // 缺少的末尾字段被置零
    REQUIRE(p->max_delta_rssi == 0);
  }
}
