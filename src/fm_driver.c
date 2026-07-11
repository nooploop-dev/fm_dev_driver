#include "fm_crc.h"
#include "fm_driver.h"
#include "fm_driver_raw.h"
#include "fm_frame.h"
#include "fm_msg.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

// 无线封装消息不对外暴露，但协议层仍使用固定 ID。
enum {
  FM_MSG_INTERNAL_DATA_USER_TO_DEV = 34,
  FM_MSG_INTERNAL_DATA_DEV_TO_USER = 35,
};

// encode: 表示将消息结构体从app层(FMData*)转换为协议层(FMRawData*)
// decode: 表示将消息结构体从协议层(FMRawData*)转换为app层(FMData*)

static int fm_user_to_dev_encode(fm_msg_id_t msg_id, const void *app,
                                 uint8_t *out) {
  int size = 0;
  switch (msg_id) {
  case FM_MSG_ECHO:
    fm_data_echo_to_raw(app, out, &size);
    break;
  case FM_MSG_FIND:
    fm_data_find_to_raw(app, out, &size);
    break;
  case FM_MSG_RESTART:
    fm_data_restart_to_raw(app, out, &size);
    break;
  case FM_MSG_PARAM_READ:
    break;
  case FM_MSG_PARAM_WRITE:
    fm_data_param_to_raw(app, out, &size);
    break;
  case FM_MSG_BEGIN_PAIR:
    fm_data_begin_pair_to_raw(app, out, &size);
    break;
  case FM_MSG_CANCEL_PAIR:
    fm_data_cancel_pair_to_raw(app, out, &size);
    break;
  case FM_MSG_DATA_USER_TO_USER:
    fm_data_user_to_user_to_raw(app, out, &size);
    break;
  default:
    assert(0 && "msg_id is not valid for user->dev direction");
    break;
  }
  return size;
}

// 将对外msg(FMData*)按 dev->user 方向转换为协议payload，返回payload大小
static int fm_dev_to_user_encode(fm_msg_id_t msg_id, const void *app,
                                 uint8_t *out) {
  int size = 0;
  switch (msg_id) {
  case FM_MSG_ECHO:
    fm_data_echo_to_raw(app, out, &size);
    break;
  case FM_MSG_FIND:
    fm_data_find_to_raw(app, out, &size);
    break;
  case FM_MSG_RESTART:
    fm_data_restart_to_raw(app, out, &size);
    break;
  case FM_MSG_PARAM: // 响应
    fm_data_param_to_raw(app, out, &size);
    break;
  case FM_MSG_HEARTBEAT:
    fm_data_heartbeat_to_raw(app, out, &size);
    break;
  case FM_MSG_RESULT:
    fm_data_result_to_raw(app, out, &size);
    break;
  case FM_MSG_PREV_RESULT:
    fm_data_prev_result_to_raw(app, out, &size);
    break;
  case FM_MSG_BEGIN_PAIR:
    fm_data_begin_pair_to_raw(app, out, &size);
    break;
  case FM_MSG_CANCEL_PAIR:
    fm_data_cancel_pair_to_raw(app, out, &size);
    break;
  case FM_MSG_DATA_USER_TO_USER:
    fm_data_user_to_user_to_raw(app, out, &size);
    break;
  case FM_MSG_SPHERICAL_RESULT:
    fm_data_spherical_result_to_raw(app, out, &size);
    break;
  case FM_MSG_PREV_SPHERICAL_RESULT:
    fm_data_spherical_prev_result_to_raw(app, out, &size);
    break;
  case FM_MSG_DIS:
    fm_data_dis_to_raw(app, out, &size);
    break;
  default:
    assert(0 && "msg_id is not valid for dev->user direction");
    break;
  }
  return size;
}

#define FM_DEV_TO_USER_DISPATCH(PROTO, FROM, APPTYPE)                          \
  do {                                                                         \
    PROTO p;                                                                   \
    fm_msg_copy_payload(&p, sizeof(p), msg->payload, msg->payload_size);       \
    APPTYPE a;                                                                 \
    FROM(&p, &a);                                                              \
    if (parser->on_frame_msg) {                                                \
      parser->on_frame_msg(parser->connect_type, msg->id, &a, (int)sizeof(a)); \
    }                                                                          \
  } while (0)

static void fm_dev_to_user_decode(void *arg, const FMData *msg);

// dev_to_user的payload中嵌套了来自无线侧的msg，需要再次解析
static void fm_parser_from_dev_dev_to_user(FMParserFromDev *parser,
                                           const FMData *msg) {
  const FMRawDataDataDevToUser *ud =
      (const FMRawDataDataDevToUser *)msg->payload;
  parser->connect_type = FM_WIRELESS;
  fm_msg_parser_handle_data(&fm_dev_to_user_decode, parser, ud->payload,
                            ud->payload_size);
  // 恢复瞬时状态，同帧后续消息仍属于有线直连节点
  parser->connect_type = FM_WIRED;
}

static void fm_parser_from_dev_user_to_user(FMParserFromDev *parser,
                                            const FMData *msg) {
  FMRawDataDataUserToUser ud;
  fm_msg_copy_payload(&ud, sizeof(ud), msg->payload, msg->payload_size);
  FMDataDataUserToUser a;
  fm_data_user_to_user_from_raw(&ud, &a);
  if (parser->on_frame_msg) {
    // user_to_user 恒为无线对端用户的数据
    parser->on_frame_msg(FM_WIRELESS, FM_MSG_DATA_USER_TO_USER, &a,
                         (int)sizeof(a));
  }
}

static void fm_dev_to_user_decode(void *arg, const FMData *msg) {
  FMParserFromDev *parser = (FMParserFromDev *)arg;
  switch (msg->id) {
  case FM_MSG_ECHO:
    FM_DEV_TO_USER_DISPATCH(FMRawDataEcho, fm_data_echo_from_raw, FMDataEcho);
    break;
  case FM_MSG_FIND:
    FM_DEV_TO_USER_DISPATCH(FMRawDataFind, fm_data_find_from_raw, FMDataFind);
    break;
  case FM_MSG_RESTART:
    FM_DEV_TO_USER_DISPATCH(FMRawDataRestart, fm_data_restart_from_raw,
                            FMDataRestart);
    break;
  case FM_MSG_HEARTBEAT:
    FM_DEV_TO_USER_DISPATCH(FMRawDataHeartbeat, fm_data_heartbeat_from_raw,
                            FMDataHeartbeat);
    break;
  case FM_MSG_RESULT:
    FM_DEV_TO_USER_DISPATCH(FMRawDataResult, fm_data_result_from_raw,
                            FMDataResult);
    break;
  case FM_MSG_PREV_RESULT:
    FM_DEV_TO_USER_DISPATCH(FMRawDataPrevResult, fm_data_prev_result_from_raw,
                            FMDataPrevResult);
    break;
  case FM_MSG_DIS:
    FM_DEV_TO_USER_DISPATCH(FMRawDataDis, fm_data_dis_from_raw, FMDataDis);
    break;
  case FM_MSG_BEGIN_PAIR:
    FM_DEV_TO_USER_DISPATCH(FMRawDataBeginPair, fm_data_begin_pair_from_raw,
                            FMDataBeginPair);
    break;
  case FM_MSG_CANCEL_PAIR:
    FM_DEV_TO_USER_DISPATCH(FMRawDataCancelPair, fm_data_cancel_pair_from_raw,
                            FMDataCancelPair);
    break;
  case FM_MSG_SPHERICAL_RESULT:
    FM_DEV_TO_USER_DISPATCH(FMRawDataSphericalResult,
                            fm_data_spherical_result_from_raw,
                            FMDataSphericalResult);
    break;
  case FM_MSG_PREV_SPHERICAL_RESULT:
    FM_DEV_TO_USER_DISPATCH(FMRawDataPrevSphericalResult,
                            fm_data_spherical_prev_result_from_raw,
                            FMDataSphericalPrevResult);
    break;
  case FM_MSG_PARAM:
    FM_DEV_TO_USER_DISPATCH(FMRawDataParam, fm_data_param_from_raw,
                            FMDataParam);
    break;
  case FM_MSG_INTERNAL_DATA_DEV_TO_USER:
    fm_parser_from_dev_dev_to_user(parser, msg);
    break;
  case FM_MSG_DATA_USER_TO_USER:
    fm_parser_from_dev_user_to_user(parser, msg);
    break;
  default:
    break;
  }
}

#define FM_USER_TO_DEV_DISPATCH(PROTO, FROM, APPTYPE)                          \
  do {                                                                         \
    PROTO p;                                                                   \
    fm_msg_copy_payload(&p, sizeof(p), msg->payload, msg->payload_size);       \
    APPTYPE a;                                                                 \
    FROM(&p, &a);                                                              \
    if (parser->on_frame_msg) {                                                \
      parser->on_frame_msg(parser->connect_type, msg->id, &a, (int)sizeof(a)); \
    }                                                                          \
  } while (0)

static void fm_user_to_dev_decode(void *arg, const FMData *msg);

// user_to_dev的payload中嵌套了发往无线侧的msg，需要再次解析
static void fm_parser_from_user_user_to_dev(FMParserFromUser *parser,
                                            const FMData *msg) {
  const FMRawDataDataUserToDev *ud =
      (const FMRawDataDataUserToDev *)msg->payload;
  parser->connect_type = FM_WIRELESS;
  fm_msg_parser_handle_data(&fm_user_to_dev_decode, parser, ud->payload,
                            ud->payload_size);
  // 恢复瞬时状态，同帧后续消息仍属于有线直连节点
  parser->connect_type = FM_WIRED;
}

static void fm_parser_from_user_user_to_user(FMParserFromUser *parser,
                                             const FMData *msg) {
  FMRawDataDataUserToUser ud;
  fm_msg_copy_payload(&ud, sizeof(ud), msg->payload, msg->payload_size);
  FMDataDataUserToUser a;
  fm_data_user_to_user_from_raw(&ud, &a);
  if (parser->on_frame_msg) {
    parser->on_frame_msg(parser->connect_type, FM_MSG_DATA_USER_TO_USER, &a,
                         (int)sizeof(a));
  }
}

static void fm_user_to_dev_decode(void *arg, const FMData *msg) {
  FMParserFromUser *parser = (FMParserFromUser *)arg;
  switch (msg->id) {
  // 无负载的读取命令
  case FM_MSG_PARAM_READ:
    if (parser->on_frame_msg) {
      parser->on_frame_msg(parser->connect_type, msg->id, NULL, 0);
    }
    break;
  case FM_MSG_ECHO:
    FM_USER_TO_DEV_DISPATCH(FMRawDataEcho, fm_data_echo_from_raw, FMDataEcho);
    break;
  case FM_MSG_FIND:
    FM_USER_TO_DEV_DISPATCH(FMRawDataFind, fm_data_find_from_raw, FMDataFind);
    break;
  case FM_MSG_RESTART:
    FM_USER_TO_DEV_DISPATCH(FMRawDataRestart, fm_data_restart_from_raw,
                            FMDataRestart);
    break;
  case FM_MSG_BEGIN_PAIR:
    FM_USER_TO_DEV_DISPATCH(FMRawDataBeginPair, fm_data_begin_pair_from_raw,
                            FMDataBeginPair);
    break;
  case FM_MSG_CANCEL_PAIR:
    FM_USER_TO_DEV_DISPATCH(FMRawDataCancelPair, fm_data_cancel_pair_from_raw,
                            FMDataCancelPair);
    break;
  case FM_MSG_PARAM:
    FM_USER_TO_DEV_DISPATCH(FMRawDataParam, fm_data_param_from_raw,
                            FMDataParam);
    break;
  case FM_MSG_INTERNAL_DATA_USER_TO_DEV:
    fm_parser_from_user_user_to_dev(parser, msg);
    break;
  case FM_MSG_DATA_USER_TO_USER:
    fm_parser_from_user_user_to_user(parser, msg);
    break;
  default:
    break;
  }
}

// 将单条消息直接构建到out(容量out_size_max)，任何写入前先校验容量，
// 容量不足时不写入并返回0，成功返回写入的字节数
static int fm_build_msg_to_dev(fm_connect_type_e connect_type,
                               fm_msg_id_t msg_id, const void *msg_payload,
                               uint8_t *out, int out_size_max) {
  uint8_t raw_data[FM_MSG_PAYLOAD_SIZE_MAX];
  int raw_data_size = fm_user_to_dev_encode(msg_id, msg_payload, raw_data);
  if (connect_type == FM_WIRED) {
    if (FM_MSG_HEADER_SIZE + raw_data_size > out_size_max) {
      return 0;
    }
    return fm_msg_write(out, msg_id, raw_data, raw_data_size);
  }
  // 无线下发需要先包一层user_data: 按最终布局在out上就地写入
  FMData *outer = (FMData *)out;
  FMRawDataDataUserToDev *ud = (FMRawDataDataUserToDev *)outer->payload;
  int ud_size = offsetof(FMRawDataDataUserToDev, payload) + FM_MSG_HEADER_SIZE +
                raw_data_size;
  if (FM_MSG_HEADER_SIZE + ud_size > out_size_max) {
    return 0;
  }
  ud->payload_size =
      (uint8_t)fm_msg_write(ud->payload, msg_id, raw_data, raw_data_size);
  outer->id = FM_MSG_INTERNAL_DATA_USER_TO_DEV;
  outer->payload_size = ud_size;
  return FM_MSG_HEADER_SIZE + ud_size;
}

int fm_prepare_msg_to_dev(fm_connect_type_e connect_type,
                          fm_frame_cnt_t frame_cnt, fm_msg_id_t msg_id,
                          const void *msg_payload, void *frame,
                          int frame_size_max) {
  fm_prepare_msg_to_dev_begin(frame_cnt, frame, frame_size_max);
  if (!fm_prepare_msg_to_dev_try_append(connect_type, msg_id, msg_payload,
                                        frame, frame_size_max)) {
    return 0;
  }
  return fm_prepare_msg_to_dev_end(frame, frame_size_max);
}

void fm_prepare_msg_to_dev_begin(fm_frame_cnt_t frame_cnt, void *frame,
                                 int frame_size_max) {
  (void)frame_size_max;
  FMFrameUserToDev *f = (FMFrameUserToDev *)frame;
  f->sof = FM_FRAME_SOF;
  f->cnt = frame_cnt;
  f->payload_size = 0;
}

bool fm_prepare_msg_to_dev_try_append(fm_connect_type_e connect_type,
                                      fm_msg_id_t msg_id,
                                      const void *msg_payload, void *frame,
                                      int frame_size_max) {
  FMFrameUserToDev *f = (FMFrameUserToDev *)frame;
  int f_payload_available = frame_size_max - fm_frame_user_to_dev_bytes(f);
  int appended =
      fm_build_msg_to_dev(connect_type, msg_id, msg_payload,
                          f->payload + f->payload_size, f_payload_available);
  if (appended <= 0) {
    return false;
  }
  f->payload_size = f->payload_size + appended;
  return true;
}

int fm_prepare_msg_to_dev_end(void *frame, int frame_size_max) {
  (void)frame_size_max;
  FMFrameUserToDev *f = (FMFrameUserToDev *)frame;
  fm_crc16_update(f, fm_frame_user_to_dev_bytes(f));
  return fm_frame_user_to_dev_bytes(f);
}

// 将单条消息直接构建到out(容量out_size_max)，任何写入前先校验容量，
// 容量不足时不写入并返回0，成功返回写入的字节数
static int fm_build_msg_to_user(fm_connect_type_e connect_type,
                                fm_msg_id_t msg_id, const void *msg_payload,
                                uint8_t *out, int out_size_max) {
  uint8_t raw_data[FM_MSG_PAYLOAD_SIZE_MAX];
  int raw_data_size = fm_dev_to_user_encode(msg_id, msg_payload, raw_data);
  if (connect_type == FM_WIRED) {
    if (FM_MSG_HEADER_SIZE + raw_data_size > out_size_max) {
      return 0;
    }
    return fm_msg_write(out, msg_id, raw_data, raw_data_size);
  }
  // 无线侧消息需要先包一层dev_to_user: 按最终布局在out上就地写入
  FMData *outer = (FMData *)out;
  FMRawDataDataDevToUser *ud = (FMRawDataDataDevToUser *)outer->payload;
  int ud_size = offsetof(FMRawDataDataDevToUser, payload) + FM_MSG_HEADER_SIZE +
                raw_data_size;
  if (FM_MSG_HEADER_SIZE + ud_size > out_size_max) {
    return 0;
  }
  ud->payload_size =
      (uint8_t)fm_msg_write(ud->payload, msg_id, raw_data, raw_data_size);
  outer->id = FM_MSG_INTERNAL_DATA_DEV_TO_USER;
  outer->payload_size = ud_size;
  return FM_MSG_HEADER_SIZE + ud_size;
}

int fm_prepare_msg_to_user(fm_role_e wired_role, const uint8_t *wired_uid,
                           fm_frame_cnt_t frame_cnt,
                           fm_connect_type_e connect_type, fm_msg_id_t msg_id,
                           const void *msg_payload, void *frame,
                           int frame_size_max) {
  fm_prepare_msg_to_user_begin(wired_role, wired_uid, frame_cnt, frame,
                               frame_size_max);
  if (!fm_prepare_msg_to_user_try_append(connect_type, msg_id, msg_payload,
                                         frame, frame_size_max)) {
    return 0;
  }
  return fm_prepare_msg_to_user_end(frame, frame_size_max);
}

void fm_prepare_msg_to_user_begin(fm_role_e wired_role,
                                  const uint8_t *wired_uid,
                                  fm_frame_cnt_t frame_cnt, void *frame,
                                  int frame_size_max) {
  (void)frame_size_max;
  FMFrameDevToUser *f = (FMFrameDevToUser *)frame;
  f->sof = FM_FRAME_SOF;
  f->sys_type = FM_SYS_TYPE;
  f->role = (uint8_t)wired_role;
  f->cnt = frame_cnt;
  if (wired_uid) {
    memcpy(f->uid, wired_uid, FM_UID_SIZE);
  } else {
    memset(f->uid, 0, FM_UID_SIZE);
  }
  f->payload_size = 0;
}

bool fm_prepare_msg_to_user_try_append(fm_connect_type_e connect_type,
                                       fm_msg_id_t msg_id,
                                       const void *msg_payload, void *frame,
                                       int frame_size_max) {
  FMFrameDevToUser *f = (FMFrameDevToUser *)frame;
  int f_payload_available = frame_size_max - fm_frame_dev_to_user_bytes(f);
  int appended =
      fm_build_msg_to_user(connect_type, msg_id, msg_payload,
                           f->payload + f->payload_size, f_payload_available);
  if (appended <= 0) {
    return false;
  }
  f->payload_size = f->payload_size + appended;
  return true;
}

int fm_prepare_msg_to_user_end(void *frame, int frame_size_max) {
  (void)frame_size_max;
  FMFrameDevToUser *f = (FMFrameDevToUser *)frame;
  fm_crc16_update(f, fm_frame_dev_to_user_bytes(f));
  return fm_frame_dev_to_user_bytes(f);
}

static void fm_parser_from_dev_on_frame(void *arg, const void *frame,
                                        int frame_size) {
  (void)frame_size;
  FMParserFromDev *parser = (FMParserFromDev *)arg;
  const FMFrameDevToUser *f = (const FMFrameDevToUser *)frame;
  parser->connect_type = FM_WIRED;
  parser->wired_role = f->role;
  parser->frame_cnt = f->cnt;
  memcpy(parser->wired_uid, f->uid, FM_UID_SIZE);
  if (parser->on_frame_begin) {
    parser->on_frame_begin(parser->wired_role, parser->wired_uid,
                           parser->frame_cnt);
  }
  fm_msg_parser_handle_data(&fm_dev_to_user_decode, parser, f->payload,
                            f->payload_size);
  if (parser->on_frame_end) {
    parser->on_frame_end();
  }
}

void fm_parser_from_dev_init(FMParserFromDev *parser,
                             fm_on_frame_begin_from_dev_f on_frame_begin,
                             fm_on_frame_msg_from_dev_f on_frame_msg,
                             fm_on_frame_end_from_dev_f on_frame_end) {
  memset(parser, 0, sizeof(*parser));
  parser->on_frame_begin = on_frame_begin;
  parser->on_frame_msg = on_frame_msg;
  parser->on_frame_end = on_frame_end;
}

void fm_parser_from_dev_handle_data(FMParserFromDev *parser, const void *data,
                                    int data_size) {
  fm_frame_buffer_feed(parser->buffer, &parser->index_begin, &parser->index_end,
                       &fm_frame_dev_to_user_layout, data, data_size,
                       fm_parser_from_dev_on_frame, parser);
}

static void fm_parser_from_user_on_frame(void *arg, const void *frame,
                                         int frame_size) {
  (void)frame_size;
  FMParserFromUser *parser = (FMParserFromUser *)arg;
  const FMFrameUserToDev *f = (const FMFrameUserToDev *)frame;
  parser->connect_type = FM_WIRED;
  parser->frame_cnt = f->cnt;
  if (parser->on_frame_begin) {
    parser->on_frame_begin(parser->frame_cnt);
  }
  fm_msg_parser_handle_data(&fm_user_to_dev_decode, parser, f->payload,
                            f->payload_size);
  if (parser->on_frame_end) {
    parser->on_frame_end();
  }
}

void fm_parser_from_user_init(FMParserFromUser *parser,
                              fm_on_frame_begin_from_user_f on_frame_begin,
                              fm_on_frame_msg_from_user_f on_frame_msg,
                              fm_on_frame_end_from_user_f on_frame_end) {
  memset(parser, 0, sizeof(*parser));
  parser->on_frame_begin = on_frame_begin;
  parser->on_frame_msg = on_frame_msg;
  parser->on_frame_end = on_frame_end;
}

void fm_parser_from_user_handle_data(FMParserFromUser *parser, const void *data,
                                     int data_size) {
  fm_frame_buffer_feed(parser->buffer, &parser->index_begin, &parser->index_end,
                       &fm_frame_user_to_dev_layout, data, data_size,
                       fm_parser_from_user_on_frame, parser);
}
