// 设备端需要使用的接口
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "fm_driver.h"

/**
 * @brief 将一个msg封装为一帧，设备端随后可直接把frame发送给用户
 *
 * @param wired_role 设备自身角色(FM_ANCHOR/FM_TAG)
 * @param wired_uid 设备自身uid(长度FM_UID_SIZE)，可传NULL表示全0
 * @param frame_cnt 帧计数，每帧需+1
 * @param connect_type 消息来源的连接类型
 * @param msg_id 消息id(FM_MSG_*)
 * @param msg_payload 对应msg_id的结构体指针(FMData*)
 * @param frame 输出缓冲区
 * @param frame_size_max frame的容量
 * @return 封装好的帧大小
 */
int fm_prepare_msg_to_user(fm_role_e wired_role, const uint8_t *wired_uid,
                           fm_frame_cnt_t frame_cnt,
                           fm_connect_type_e connect_type, fm_msg_id_t msg_id,
                           const void *msg_payload, void *frame,
                           int frame_size_max);

// 以下为分步骤接口，适用于一帧中包含多个msg的情况
// 用法: begin 初始化 -> try_append 追加(可多次) -> end
// 收尾，frame须为同一缓冲区

/**
 * @brief 开始构建一帧发往用户的数据，初始化帧头(frame内容会被重置)
 *
 * @param wired_role 设备自身角色(FM_ANCHOR/FM_TAG)
 * @param wired_uid 设备自身uid(长度FM_UID_SIZE)，可传NULL表示全0
 * @param frame_cnt 帧计数，每帧需+1
 * @param frame 输出缓冲区，begin/try_append/end须传入同一个
 * @param frame_size_max frame的容量
 */
void fm_prepare_msg_to_user_begin(fm_role_e wired_role,
                                  const uint8_t *wired_uid,
                                  fm_frame_cnt_t frame_cnt, void *frame,
                                  int frame_size_max);
/**
 * @brief 向当前帧追加一个msg
 *
 * @param connect_type 消息来源的连接类型
 * @param msg_id 消息id(FM_MSG_*)
 * @param msg_payload 对应msg_id的结构体指针(FMData*)
 * @param frame 输出缓冲区，同begin传入的那个
 * @param frame_size_max frame的容量
 * @return true:追加成功; false:剩余空间不足，未追加(应调用end结束当前帧)
 */
bool fm_prepare_msg_to_user_try_append(fm_connect_type_e connect_type,
                                       fm_msg_id_t msg_id,
                                       const void *msg_payload, void *frame,
                                       int frame_size_max);
/**
 * @brief 结束当前帧构建，计算校验，随后可直接把frame发送给用户
 *
 * @param frame 输出缓冲区，同begin传入的那个
 * @param frame_size_max frame的容量
 * @return 封装好的帧大小
 */
int fm_prepare_msg_to_user_end(void *frame, int frame_size_max);

/**
 * @brief 每收到一帧，处理其中的消息之前回调
 *
 * @param frame_cnt 该帧的帧计数
 */
typedef void (*fm_on_frame_begin_from_user_f)(fm_frame_cnt_t frame_cnt);

/**
 * @brief 每从帧中解析出一条消息回调一次
 *
 * @param connect_type 消息来源的连接类型
 * @param msg_id 消息id(FM_MSG_*)
 * @param msg_payload 对应msg_id的结构体指针(FMData*)
 * @param msg_payload_size 消息负载大小
 */
typedef void (*fm_on_frame_msg_from_user_f)(fm_connect_type_e connect_type,
                                            fm_msg_id_t msg_id,
                                            const void *msg_payload,
                                            int msg_payload_size);

/**
 * @brief 一帧内所有消息处理完毕后回调
 */
typedef void (*fm_on_frame_end_from_user_f)(void);

typedef struct {
  uint8_t buffer[FM_FRAME_SIZE_MAX];
  int index_begin;
  int index_end;
  fm_on_frame_begin_from_user_f on_frame_begin;
  fm_on_frame_msg_from_user_f on_frame_msg;
  fm_on_frame_end_from_user_f on_frame_end;
  // 以下为处理当前帧时的瞬时状态
  fm_frame_cnt_t frame_cnt;       // 当前帧计数
  fm_connect_type_e connect_type; // 当前msg的连接类型
} FMParserFromUser;
/**
 * @brief 初始化解析器并注册回调
 *
 * @param parser 解析器实例
 * @param on_frame_begin 每收到一帧，处理其中的消息之前回调，不需要可传NULL
 * @param on_frame_msg 每解析出一条消息回调一次，不需要可传NULL
 * @param on_frame_end 一帧内所有消息处理完毕后回调，不需要可传NULL
 */
void fm_parser_from_user_init(FMParserFromUser *parser,
                              fm_on_frame_begin_from_user_f on_frame_begin,
                              fm_on_frame_msg_from_user_f on_frame_msg,
                              fm_on_frame_end_from_user_f on_frame_end);
/**
 * @brief 处理收到的数据: 内部完成拆帧与消息解析，并调用on_frame_*回调
 *
 * @param parser 解析器实例
 * @param data 收到的数据
 * @param data_size data的字节数
 */
void fm_parser_from_user_handle_data(FMParserFromUser *parser, const void *data,
                                     int data_size);

#ifdef __cplusplus
}
#endif
