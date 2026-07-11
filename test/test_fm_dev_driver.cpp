#include "catch2/catch_all.hpp"
#include "fm_dev_driver.h"
#include <cstring>
#include <initializer_list>
#include <vector>

// 测试仅依赖对外的 fm_dev_driver.h:
// - user->dev 方向: fm_prepare_msg_to_dev[_*] 构造 <-> fm_parser_from_user 解析
// - dev->user 方向: fm_prepare_msg_to_user[_*] 构造 <-> fm_parser_from_dev 解析
// 回调无 arg，借助文件作用域变量捕获结果(解析夹具构造时清空)
// 为避免"构造值"与"断言值"重复书写，断言尽量比对回调结果与原始输入(同一变量)

namespace {

using Bytes = std::vector<uint8_t>;

static_assert(FM_WIRELESS == 0);
static_assert(FM_WIRED == 1);

struct FromDevMsg {
  fm_connect_type_e connect_type;
  fm_role_e role;
  uint8_t uid[FM_UID_SIZE];
  fm_frame_cnt_t cnt;
  fm_msg_id_t id;
  Bytes payload;
};
struct FromUserMsg {
  fm_connect_type_e connect_type;
  fm_frame_cnt_t cnt;
  fm_msg_id_t id;
  Bytes payload;
};

std::vector<FromDevMsg> g_from_dev;
std::vector<FromUserMsg> g_from_user;

// dev 方向: 帧头(wired_role/uid/cnt)由 on_frame_begin 提供，每个 msg 用
// connect_type + wired_role 推导所属角色，组装为一条 FromDevMsg
fm_role_e g_dev_wired_role;
uint8_t g_dev_uid[FM_UID_SIZE];
fm_frame_cnt_t g_dev_cnt;

void on_from_dev_begin(fm_role_e wired_role, const uint8_t *uid,
                       fm_frame_cnt_t cnt) {
  g_dev_wired_role = wired_role;
  std::memcpy(g_dev_uid, uid, FM_UID_SIZE);
  g_dev_cnt = cnt;
}

void on_from_dev_msg(fm_connect_type_e connect_type, fm_msg_id_t id,
                     const void *payload, int size) {
  FromDevMsg m{};
  m.connect_type = connect_type;
  m.role = connect_type == FM_WIRED
               ? g_dev_wired_role
               : (g_dev_wired_role == FM_ANCHOR ? FM_TAG : FM_ANCHOR);
  std::memcpy(m.uid, g_dev_uid, FM_UID_SIZE);
  m.cnt = g_dev_cnt;
  m.id = id;
  const uint8_t *p = static_cast<const uint8_t *>(payload);
  if (p && size > 0) {
    m.payload.assign(p, p + size);
  }
  g_from_dev.push_back(m);
}

void on_from_dev_end() {}

// user 方向: 帧计数由 on_frame_begin 提供，帧内每个 msg 复用
fm_frame_cnt_t g_user_cnt;

void on_from_user_begin(fm_frame_cnt_t cnt) { g_user_cnt = cnt; }

void on_from_user_msg(fm_connect_type_e connect_type, fm_msg_id_t id,
                      const void *payload, int size) {
  FromUserMsg m{};
  m.connect_type = connect_type;
  m.cnt = g_user_cnt;
  m.id = id;
  const uint8_t *p = static_cast<const uint8_t *>(payload);
  if (p && size > 0) {
    m.payload.assign(p, p + size);
  }
  g_from_user.push_back(m);
}

void on_from_user_end() {}

template <typename T> const T *as(const Bytes &payload) {
  return reinterpret_cast<const T *>(payload.data());
}

void append(Bytes &stream, const Bytes &v) {
  stream.insert(stream.end(), v.begin(), v.end());
}

// 把"输出缓冲 + 返回长度"收敛为一个 vector，并校验长度有效
Bytes to_bytes(const uint8_t *buf, int n) {
  REQUIRE(n > 0);
  return Bytes(buf, buf + n);
}

// 单帧构造 -------------------------------------------------------------------

Bytes build_to_dev(fm_connect_type_e ct, fm_frame_cnt_t cnt, fm_msg_id_t id,
                   const void *payload) {
  uint8_t buf[FM_FRAME_SIZE_MAX];
  return to_bytes(
      buf, fm_prepare_msg_to_dev(ct, cnt, id, payload, buf, sizeof(buf)));
}

Bytes build_to_user(fm_role_e role, const uint8_t *uid, fm_frame_cnt_t cnt,
                    fm_connect_type_e ct, fm_msg_id_t id, const void *payload) {
  uint8_t buf[FM_FRAME_SIZE_MAX];
  return to_bytes(buf, fm_prepare_msg_to_user(role, uid, cnt, ct, id, payload,
                                              buf, sizeof(buf)));
}

// 多 msg 单帧构造(begin -> try_append... -> end) -----------------------------

struct ToDevMsg {
  fm_connect_type_e ct;
  fm_msg_id_t id;
  const void *payload;
};
Bytes build_to_dev_multi(fm_frame_cnt_t cnt,
                         std::initializer_list<ToDevMsg> msgs) {
  uint8_t buf[FM_FRAME_SIZE_MAX];
  fm_prepare_msg_to_dev_begin(cnt, buf, sizeof(buf));
  for (const ToDevMsg &m : msgs) {
    REQUIRE(fm_prepare_msg_to_dev_try_append(m.ct, m.id, m.payload, buf,
                                             sizeof(buf)));
  }
  return to_bytes(buf, fm_prepare_msg_to_dev_end(buf, sizeof(buf)));
}

struct ToUserMsg {
  fm_connect_type_e ct;
  fm_msg_id_t id;
  const void *payload;
};
Bytes build_to_user_multi(fm_role_e role, const uint8_t *uid,
                          fm_frame_cnt_t cnt,
                          std::initializer_list<ToUserMsg> msgs) {
  uint8_t buf[FM_FRAME_SIZE_MAX];
  fm_prepare_msg_to_user_begin(role, uid, cnt, buf, sizeof(buf));
  for (const ToUserMsg &m : msgs) {
    REQUIRE(fm_prepare_msg_to_user_try_append(m.ct, m.id, m.payload, buf,
                                              sizeof(buf)));
  }
  return to_bytes(buf, fm_prepare_msg_to_user_end(buf, sizeof(buf)));
}

// 解析夹具: 构造时清空结果并初始化解析器，封装喂数据的两种方式 ----------------

struct FromUserParser {
  FMParserFromUser parser;
  FromUserParser() {
    g_from_user.clear();
    fm_parser_from_user_init(&parser, on_from_user_begin, on_from_user_msg,
                             on_from_user_end);
  }
  void feed(const Bytes &s) {
    fm_parser_from_user_handle_data(&parser, s.data(), (int)s.size());
  }
  void feed_byte_by_byte(const Bytes &s) {
    for (uint8_t b : s) {
      fm_parser_from_user_handle_data(&parser, &b, 1);
    }
  }
};

struct FromDevParser {
  FMParserFromDev parser;
  FromDevParser() {
    g_from_dev.clear();
    fm_parser_from_dev_init(&parser, on_from_dev_begin, on_from_dev_msg,
                            on_from_dev_end);
  }
  void feed(const Bytes &s) {
    fm_parser_from_dev_handle_data(&parser, s.data(), (int)s.size());
  }
  void feed_byte_by_byte(const Bytes &s) {
    for (uint8_t b : s) {
      fm_parser_from_dev_handle_data(&parser, &b, 1);
    }
  }
};

} // namespace

TEST_CASE("user->dev round trip") {
  FromUserParser p;

  SECTION("single msg") {
    const fm_frame_cnt_t cnt = 11;
    FMDataFind f{};
    f.duration = 7;

    p.feed(build_to_dev(FM_WIRED, cnt, FM_MSG_FIND, &f));

    REQUIRE(g_from_user.size() == 1);
    REQUIRE(g_from_user[0].id == FM_MSG_FIND);
    REQUIRE(g_from_user[0].cnt == cnt);
    REQUIRE(g_from_user[0].connect_type == FM_WIRED);
    REQUIRE(as<FMDataFind>(g_from_user[0].payload)->duration == f.duration);
  }

  SECTION("multiple msgs in one frame") {
    const fm_frame_cnt_t cnt = 22;
    FMDataFind f{};
    f.duration = 7;
    FMDataRestart r{};
    r.delay = 3;
    r.only_when_need = true;

    // 末尾追加一条无负载的 read 类消息，验证其回调 payload 为空
    p.feed(build_to_dev_multi(cnt, {
                                       {FM_WIRED, FM_MSG_FIND, &f},
                                       {FM_WIRED, FM_MSG_RESTART, &r},
                                       {FM_WIRED, FM_MSG_PARAM_READ, nullptr},
                                   }));

    REQUIRE(g_from_user.size() == 3);
    REQUIRE(g_from_user[0].id == FM_MSG_FIND);
    REQUIRE(g_from_user[0].cnt == cnt);
    REQUIRE(as<FMDataFind>(g_from_user[0].payload)->duration == f.duration);
    REQUIRE(g_from_user[1].id == FM_MSG_RESTART);
    REQUIRE(as<FMDataRestart>(g_from_user[1].payload)->delay == r.delay);
    REQUIRE(as<FMDataRestart>(g_from_user[1].payload)->only_when_need ==
            r.only_when_need);
    REQUIRE(g_from_user[2].id == FM_MSG_PARAM_READ);
    REQUIRE(g_from_user[2].payload.empty());
  }

  SECTION("wireless (wired=false) is unwrapped by parser") {
    const fm_frame_cnt_t cnt = 55;
    FMDataFind f{};
    f.duration = 42;

    // FM_WIRELESS 时 msg 被包进内部无线 user_data，
    // 解析端自动拆包后回调内层 msg，并以 wired=false 上报
    p.feed(build_to_dev(FM_WIRELESS, cnt, FM_MSG_FIND, &f));

    REQUIRE(g_from_user.size() == 1);
    REQUIRE(g_from_user[0].id == FM_MSG_FIND);
    REQUIRE(g_from_user[0].cnt == cnt);
    REQUIRE(g_from_user[0].connect_type == FM_WIRELESS);
    REQUIRE(as<FMDataFind>(g_from_user[0].payload)->duration == f.duration);
  }

  SECTION("mixed wired and wireless msgs in one frame") {
    const fm_frame_cnt_t cnt = 66;
    FMDataFind f{};
    f.duration = 7;
    FMDataRestart r{};
    r.delay = 9;
    r.only_when_need = false;

    p.feed(build_to_dev_multi(cnt, {
                                       {FM_WIRED, FM_MSG_FIND, &f},
                                       {FM_WIRELESS, FM_MSG_RESTART, &r},
                                   }));

    REQUIRE(g_from_user.size() == 2);
    REQUIRE(g_from_user[0].id == FM_MSG_FIND);
    REQUIRE(g_from_user[0].cnt == cnt);
    REQUIRE(g_from_user[0].connect_type == FM_WIRED);
    REQUIRE(as<FMDataFind>(g_from_user[0].payload)->duration == f.duration);
    REQUIRE(g_from_user[1].id == FM_MSG_RESTART);
    REQUIRE(g_from_user[1].cnt == cnt);
    REQUIRE(g_from_user[1].connect_type == FM_WIRELESS);
    REQUIRE(as<FMDataRestart>(g_from_user[1].payload)->delay == r.delay);
  }
}

TEST_CASE("dev->user round trip") {
  FromDevParser p;
  const uint8_t uid[FM_UID_SIZE] = {1, 2, 3, 4, 5, 6};

  SECTION("single msg carries role/uid") {
    const fm_frame_cnt_t cnt = 33;
    FMDataDis d{};
    d.local_time = 123456;
    d.cnt = 9;
    d.dis = 2.5f;
    d.rx_rate = 4;

    p.feed(build_to_user(FM_TAG, uid, cnt, FM_WIRED, FM_MSG_DIS, &d));

    REQUIRE(g_from_dev.size() == 1);
    const FromDevMsg &m = g_from_dev[0];
    REQUIRE(m.id == FM_MSG_DIS);
    REQUIRE(m.cnt == cnt);
    REQUIRE(m.connect_type == FM_WIRED);
    REQUIRE(m.role == FM_TAG);
    REQUIRE(std::memcmp(m.uid, uid, FM_UID_SIZE) == 0);
    const FMDataDis *dd = as<FMDataDis>(m.payload);
    REQUIRE(dd->local_time == d.local_time);
    REQUIRE(dd->cnt == d.cnt);
    REQUIRE(dd->dis == Catch::Approx(d.dis));
    REQUIRE(dd->rx_rate == d.rx_rate);
  }

  SECTION("multiple msgs in one frame") {
    const fm_frame_cnt_t cnt = 44;
    FMDataDis d{};
    d.local_time = 10;
    d.cnt = 1;
    d.dis = 1.0f;
    FMDataSphericalResult s{};
    s.local_time = 20;
    s.cnt = 2;
    s.dis = 1.25f;
    s.azimuth = 0.5f;
    s.elevation = -0.25f;

    p.feed(build_to_user_multi(FM_ANCHOR, uid, cnt,
                               {
                                   {FM_WIRED, FM_MSG_DIS, &d},
                                   {FM_WIRED, FM_MSG_SPHERICAL_RESULT, &s},
                               }));

    REQUIRE(g_from_dev.size() == 2);
    REQUIRE(g_from_dev[0].id == FM_MSG_DIS);
    REQUIRE(g_from_dev[0].cnt == cnt);
    REQUIRE(g_from_dev[0].role == FM_ANCHOR);
    REQUIRE(g_from_dev[1].id == FM_MSG_SPHERICAL_RESULT);
    const FMDataSphericalResult *ss =
        as<FMDataSphericalResult>(g_from_dev[1].payload);
    REQUIRE(ss->cnt == s.cnt);
    REQUIRE(ss->dis == Catch::Approx(s.dis));
    REQUIRE(ss->azimuth == Catch::Approx(s.azimuth));
    REQUIRE(ss->elevation == Catch::Approx(s.elevation));
  }

  SECTION("mixed wired and wireless msgs in one frame") {
    const fm_frame_cnt_t cnt = 55;
    FMDataSphericalResult s{};
    s.local_time = 20;
    s.cnt = 2;
    s.dis = 1.25f;
    FMDataDis d{};
    d.local_time = 10;
    d.cnt = 1;
    d.dis = 1.0f;

    // FM_WIRELESS 的消息被包进内部无线封装，解析端拆包后按无线来源上报
    // (角色翻转)；其后 FM_WIRED 的消息不应受前一条无线消息影响
    p.feed(build_to_user_multi(FM_ANCHOR, uid, cnt,
                               {
                                   {FM_WIRELESS, FM_MSG_SPHERICAL_RESULT, &s},
                                   {FM_WIRED, FM_MSG_DIS, &d},
                               }));

    REQUIRE(g_from_dev.size() == 2);
    REQUIRE(g_from_dev[0].id == FM_MSG_SPHERICAL_RESULT);
    REQUIRE(g_from_dev[0].connect_type == FM_WIRELESS);
    REQUIRE(g_from_dev[0].role == FM_TAG);
    REQUIRE(g_from_dev[0].cnt == cnt);
    REQUIRE(g_from_dev[1].id == FM_MSG_DIS);
    REQUIRE(g_from_dev[1].connect_type == FM_WIRED);
    REQUIRE(g_from_dev[1].role == FM_ANCHOR);
    REQUIRE(as<FMDataDis>(g_from_dev[1].payload)->cnt == d.cnt);
  }
}

TEST_CASE("from_user frame parsing") {
  FromUserParser p;

  auto find_frame = [](fm_frame_cnt_t cnt, uint8_t duration) {
    FMDataFind f{};
    f.duration = duration;
    return build_to_dev(FM_WIRED, cnt, FM_MSG_FIND, &f);
  };
  const Bytes f1 = find_frame(1, 10);
  const Bytes f2 = find_frame(2, 20);
  const Bytes f3 = find_frame(3, 30);

  SECTION("multiple frames concatenated, single call") {
    Bytes stream;
    append(stream, f1);
    append(stream, f2);
    append(stream, f3);
    p.feed(stream);

    REQUIRE(g_from_user.size() == 3);
    REQUIRE(g_from_user[0].cnt == 1);
    REQUIRE(g_from_user[1].cnt == 2);
    REQUIRE(g_from_user[2].cnt == 3);
    REQUIRE(as<FMDataFind>(g_from_user[0].payload)->duration == 10);
    REQUIRE(as<FMDataFind>(g_from_user[2].payload)->duration == 30);
  }

  SECTION("frame split across calls (byte by byte)") {
    Bytes stream;
    append(stream, f1);
    append(stream, f2);
    append(stream, f3);
    p.feed_byte_by_byte(stream);

    REQUIRE(g_from_user.size() == 3);
    REQUIRE(g_from_user[0].cnt == 1);
    REQUIRE(g_from_user[1].cnt == 2);
    REQUIRE(g_from_user[2].cnt == 3);
  }

  SECTION("garbage bytes skipped") {
    Bytes stream;
    // 前导垃圾(含一个伪SOF 0xAA)
    stream.insert(stream.end(), {0x00, 0x11, 0xAA, 0x22});
    append(stream, f1);
    // 帧间垃圾
    stream.insert(stream.end(), {0xFF, 0xAA});
    append(stream, f2);
    p.feed(stream);

    REQUIRE(g_from_user.size() == 2);
    REQUIRE(g_from_user[0].cnt == 1);
    REQUIRE(g_from_user[1].cnt == 2);
  }

  SECTION("corrupted frame (bad crc) skipped, parser resyncs") {
    Bytes bad = f1;
    bad[bad.size() - 3] ^= 0xFF; // 破坏payload字节，导致CRC失配
    Bytes stream;
    append(stream, bad);
    append(stream, f2); // 紧随其后的有效帧应被正常解析
    p.feed(stream);

    REQUIRE(g_from_user.size() == 1);
    REQUIRE(g_from_user[0].cnt == 2);
    REQUIRE(as<FMDataFind>(g_from_user[0].payload)->duration == 20);
  }
}

TEST_CASE("from_dev frame parsing") {
  FromDevParser p;
  const uint8_t uid[FM_UID_SIZE] = {9, 8, 7, 6, 5, 4};

  auto dis_frame = [&](fm_frame_cnt_t cnt, uint8_t rate) {
    FMDataDis d{};
    d.cnt = cnt;
    d.rx_rate = rate;
    d.dis = 1.0f;
    return build_to_user(FM_TAG, uid, cnt, FM_WIRED, FM_MSG_DIS, &d);
  };

  SECTION("concatenated frames fed byte by byte") {
    Bytes stream;
    append(stream, dis_frame(1, 11));
    append(stream, dis_frame(2, 22));
    p.feed_byte_by_byte(stream);

    REQUIRE(g_from_dev.size() == 2);
    REQUIRE(g_from_dev[0].cnt == 1);
    REQUIRE(g_from_dev[0].role == FM_TAG);
    REQUIRE(std::memcmp(g_from_dev[0].uid, uid, FM_UID_SIZE) == 0);
    REQUIRE(g_from_dev[1].cnt == 2);
    REQUIRE(as<FMDataDis>(g_from_dev[1].payload)->rx_rate == 22);
  }

  SECTION("garbage bytes skipped") {
    Bytes stream = {0x00, 0xAA, 0x55};
    append(stream, dis_frame(7, 70));
    p.feed(stream);

    REQUIRE(g_from_dev.size() == 1);
    REQUIRE(g_from_dev[0].cnt == 7);
    REQUIRE(as<FMDataDis>(g_from_dev[0].payload)->rx_rate == 70);
  }
}

// =============================================================================
// 全消息单条组包/解析往返测试
// 内部无线封装消息不暴露为公共 FM_MSG_*，无独立对外组包接口。
//
// 按消息方向选用对应接口:
//   user->dev(方向含 v): fm_prepare_msg_to_dev 组包 + fm_parser_from_user 解析,
//                        WIRED / WIRELESS 两种连接类型都覆盖。
//   dev->user(方向含 ^): fm_prepare_msg_to_user 组包 + fm_parser_from_dev
//                        解析。
//
// 校验: 组包->解析后, 用解析回调得到的 payload 再次组包, 应得到与原始帧逐字节
// 一致的结果(往返保真); 同时校验 id / cnt / 连接类型 / 角色 / payload 长度。
// 空消息(无对应结构体)用 NULL 组包, 解析回调 payload 应为空。
// =============================================================================
namespace {

template <typename T> Bytes payload_of(const T &v) {
  return Bytes(reinterpret_cast<const uint8_t *>(&v),
               reinterpret_cast<const uint8_t *>(&v) + sizeof(T));
}

struct MsgCase {
  const char *name;
  fm_msg_id_t id;
  Bytes payload; // 空 => 无负载消息, 用NULL组包
  bool flips_role =
      false; // user_to_user: 解析端恒按无线对端上报(role翻转,connect=WIRELESS)
};

std::vector<MsgCase> user_to_dev_cases() {
  std::vector<MsgCase> cases;

  // 无负载的读取命令(payload为空, 用NULL组包)
  cases.push_back({"PARAM_READ", FM_MSG_PARAM_READ, {}});

  FMDataEcho echo{};
  echo.payload_size = 2;
  echo.payload[0] = 0x10;
  echo.payload[1] = 0x20;
  cases.push_back({"ECHO", FM_MSG_ECHO, payload_of(echo)});

  FMDataFind find{};
  find.duration = 9;
  cases.push_back({"FIND", FM_MSG_FIND, payload_of(find)});

  FMDataRestart restart{};
  restart.delay = 4;
  restart.only_when_need = true;
  cases.push_back({"RESTART", FM_MSG_RESTART, payload_of(restart)});

  FMDataParam param{};
  param.z_expect = 1.5f;
  param.min_dis = 0.3f;
  param.min_freq_count = 3;
  param.max_delta_rssi = 10;
  cases.push_back({"PARAM_WRITE", FM_MSG_PARAM_WRITE, payload_of(param)});

  FMDataBeginPair begin{};
  begin.timeout = 15;
  cases.push_back({"BEGIN_PAIR", FM_MSG_BEGIN_PAIR, payload_of(begin)});

  FMDataCancelPair cancel{};
  cancel.timeout = 25;
  cases.push_back({"CANCEL_PAIR", FM_MSG_CANCEL_PAIR, payload_of(cancel)});

  FMDataDataUserToUser u2u{};
  u2u.payload_size = 4;
  for (int i = 0; i < 4; i++) {
    u2u.payload[i] = (uint8_t)(i + 1);
  }
  cases.push_back(
      {"DATA_USER_TO_USER", FM_MSG_DATA_USER_TO_USER, payload_of(u2u)});

  return cases;
}

std::vector<MsgCase> dev_to_user_cases() {
  std::vector<MsgCase> cases;

  FMDataEcho echo{};
  echo.payload_size = 1;
  echo.payload[0] = 0x7E;
  cases.push_back({"ECHO", FM_MSG_ECHO, payload_of(echo)});

  FMDataFind find{};
  find.duration = 12;
  cases.push_back({"FIND", FM_MSG_FIND, payload_of(find)});

  FMDataRestart restart{};
  restart.delay = 6;
  restart.only_when_need = false;
  cases.push_back({"RESTART", FM_MSG_RESTART, payload_of(restart)});

  FMDataParam param{};
  param.z_expect = -0.75f;
  param.min_freq_count = 5;
  param.max_delta_rssi = 20;
  cases.push_back({"PARAM", FM_MSG_PARAM, payload_of(param)});

  FMDataHeartbeat hb{};
  hb.hardware.version[0] = 1;
  hb.hardware.version[1] = 2;
  hb.hardware.batch = 3;
  hb.firmware.series = 4;
  hb.firmware.version[0] = 5;
  hb.need_restart = true;
  hb.uptime = 123456;
  cases.push_back({"HEARTBEAT", FM_MSG_HEARTBEAT, payload_of(hb)});

  FMDataResult result{};
  result.local_time = 1000;
  result.cnt = 7;
  result.pos[0] = 1.0f;
  result.pos[1] = 2.0f;
  result.pos[2] = 3.0f;
  result.vel[0] = 0.1f;
  cases.push_back({"RESULT", FM_MSG_RESULT, payload_of(result)});

  FMDataPrevResult prev{};
  prev.cnt = 8;
  prev.pos[0] = 4.0f;
  prev.pos[1] = 5.0f;
  prev.pos[2] = 6.0f;
  cases.push_back({"PREV_RESULT", FM_MSG_PREV_RESULT, payload_of(prev)});

  FMDataBeginPair begin{};
  begin.timeout = 16;
  cases.push_back({"BEGIN_PAIR", FM_MSG_BEGIN_PAIR, payload_of(begin)});

  FMDataCancelPair cancel{};
  cancel.timeout = 26;
  cases.push_back({"CANCEL_PAIR", FM_MSG_CANCEL_PAIR, payload_of(cancel)});

  FMDataDataUserToUser u2u{};
  u2u.payload_size = 3;
  u2u.payload[0] = 9;
  u2u.payload[1] = 8;
  u2u.payload[2] = 7;
  cases.push_back(
      {"DATA_USER_TO_USER", FM_MSG_DATA_USER_TO_USER, payload_of(u2u), true});

  FMDataSphericalResult sph{};
  sph.local_time = 2000;
  sph.cnt = 9;
  sph.dis = 1.25f;
  sph.azimuth = 0.5f;
  sph.elevation = -0.25f;
  cases.push_back(
      {"SPHERICAL_RESULT", FM_MSG_SPHERICAL_RESULT, payload_of(sph)});

  FMDataSphericalPrevResult psph{};
  psph.cnt = 10;
  psph.dis = 2.5f;
  psph.azimuth = 1.0f;
  psph.elevation = -1.0f;
  cases.push_back({"PREV_SPHERICAL_RESULT", FM_MSG_PREV_SPHERICAL_RESULT,
                   payload_of(psph)});

  FMDataDis dis{};
  dis.local_time = 3000;
  dis.cnt = 11;
  dis.dis = 3.5f;
  dis.rx_rate = 42;
  cases.push_back({"DIS", FM_MSG_DIS, payload_of(dis)});

  return cases;
}

void check_user_to_dev(fm_connect_type_e ct, const MsgCase &c) {
  INFO("user->dev msg=" << c.name
                        << " ct=" << (ct == FM_WIRED ? "WIRED" : "WIRELESS"));
  const fm_frame_cnt_t cnt = 0x5A;
  FromUserParser p;
  const void *in = c.payload.empty() ? nullptr : c.payload.data();
  Bytes frame1 = build_to_dev(ct, cnt, c.id, in);
  p.feed(frame1);

  REQUIRE(g_from_user.size() == 1);
  REQUIRE(g_from_user[0].id == c.id);
  REQUIRE(g_from_user[0].cnt == cnt);
  REQUIRE(g_from_user[0].connect_type == ct);
  REQUIRE(g_from_user[0].payload.size() == c.payload.size());

  // 往返保真: 用解析得到的payload重新组包, 应与原始帧逐字节一致
  const void *dec =
      g_from_user[0].payload.empty() ? nullptr : g_from_user[0].payload.data();
  Bytes frame2 = build_to_dev(ct, cnt, c.id, dec);
  REQUIRE(frame1 == frame2);
}

void check_dev_to_user(fm_role_e role, const uint8_t *uid, fm_connect_type_e ct,
                       const MsgCase &c) {
  INFO("dev->user msg=" << c.name
                        << " ct=" << (ct == FM_WIRED ? "WIRED" : "WIRELESS"));
  const fm_frame_cnt_t cnt = 0x5A;
  FromDevParser p;
  const void *in = c.payload.empty() ? nullptr : c.payload.data();
  Bytes frame1 = build_to_user(role, uid, cnt, ct, c.id, in);
  p.feed(frame1);

  // user_to_user 无论怎么组包解析端都按无线对端上报，其余按组包时的连接类型
  const fm_connect_type_e expect_ct = c.flips_role ? FM_WIRELESS : ct;
  const fm_role_e expect_role =
      expect_ct == FM_WIRED ? role : (role == FM_ANCHOR ? FM_TAG : FM_ANCHOR);
  REQUIRE(g_from_dev.size() == 1);
  REQUIRE(g_from_dev[0].id == c.id);
  REQUIRE(g_from_dev[0].cnt == cnt);
  REQUIRE(g_from_dev[0].connect_type == expect_ct);
  REQUIRE(g_from_dev[0].role == expect_role);
  REQUIRE(std::memcmp(g_from_dev[0].uid, uid, FM_UID_SIZE) == 0);
  REQUIRE(g_from_dev[0].payload.size() == c.payload.size());

  const void *dec =
      g_from_dev[0].payload.empty() ? nullptr : g_from_dev[0].payload.data();
  Bytes frame2 = build_to_user(role, uid, cnt, ct, c.id, dec);
  REQUIRE(frame1 == frame2);
}

} // namespace

TEST_CASE("user->dev: every message single round trip (WIRED & WIRELESS)") {
  for (const MsgCase &c : user_to_dev_cases()) {
    check_user_to_dev(FM_WIRED, c);
    check_user_to_dev(FM_WIRELESS, c);
  }
}

TEST_CASE("dev->user: every message single round trip (WIRED & WIRELESS)") {
  const uint8_t uid[FM_UID_SIZE] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
  for (const MsgCase &c : dev_to_user_cases()) {
    check_dev_to_user(FM_TAG, uid, FM_WIRED, c);
    check_dev_to_user(FM_TAG, uid, FM_WIRELESS, c);
  }
}
