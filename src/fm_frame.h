// 定义数据帧格式及相关处理函数
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "fm_driver_common.h"
#include "fm_driver_data.h"
#include <stddef.h>
#include <stdint.h>

// 帧头
typedef uint8_t fm_frame_sof_t;
#define FM_FRAME_SOF 0xAA

// 系统类型
#define FM_SYS_TYPE 6
#define FM_SYS_TYPE_BITS 4

#define FM_ROLE_BITS 4

typedef uint16_t fm_frame_payload_size_t;
typedef uint16_t fm_frame_checksum_t;

// 需要嵌入到user_data通过uwb发送的消息的最大负载长度
#define FM_UWB_MSG_PAYLOAD_SIZE_MAX (FM_USER_DATA_PAYLOAD_SIZE - 2)
// 仅需要通过uart发送的消息的最大负载长度
#define FM_UART_MSG_PAYLOAD_SIZE_MAX 255

#pragma pack(push, 1)

// 设备发送给用户的帧
#define FM_FRAME_DEV_TO_USER_SIZE_MIN                                          \
  ((int)(sizeof(fm_frame_sof_t) + 1 + sizeof(fm_frame_cnt_t) + FM_UID_SIZE +   \
         sizeof(fm_frame_payload_size_t) + sizeof(fm_frame_checksum_t)))
#define FM_FRAME_DEV_TO_USER_PAYLOAD_SIZE_MAX                                  \
  (FM_FRAME_SIZE_MAX - FM_FRAME_DEV_TO_USER_SIZE_MIN)
typedef struct {
  fm_frame_sof_t sof;
  uint8_t sys_type : FM_SYS_TYPE_BITS;
  uint8_t role : FM_ROLE_BITS;
  fm_frame_cnt_t cnt;
  uint8_t uid[FM_UID_SIZE];
  fm_frame_payload_size_t payload_size;
  uint8_t payload[FM_FRAME_DEV_TO_USER_PAYLOAD_SIZE_MAX];
  fm_frame_checksum_t checksum;
} FMFrameDevToUser;
static inline int fm_frame_dev_to_user_bytes(const FMFrameDevToUser *f) {
  return FM_FRAME_DEV_TO_USER_SIZE_MIN + f->payload_size;
}

// 用户发送给设备的帧
#define FM_FRAME_USER_TO_DEV_SIZE_MIN                                          \
  ((int)(sizeof(fm_frame_sof_t) + sizeof(fm_frame_cnt_t) +                     \
         sizeof(fm_frame_payload_size_t) + sizeof(fm_frame_checksum_t)))
#define FM_FRAME_USER_TO_DEV_PAYLOAD_SIZE_MAX                                  \
  (FM_FRAME_SIZE_MAX - FM_FRAME_USER_TO_DEV_SIZE_MIN)
typedef struct {
  fm_frame_sof_t sof;
  fm_frame_cnt_t cnt;
  fm_frame_payload_size_t payload_size;
  uint8_t payload[FM_FRAME_USER_TO_DEV_PAYLOAD_SIZE_MAX];
  fm_frame_checksum_t checksum;
} FMFrameUserToDev;
static inline int fm_frame_user_to_dev_bytes(const FMFrameUserToDev *f) {
  return FM_FRAME_USER_TO_DEV_SIZE_MIN + f->payload_size;
}

#pragma pack(pop)

// 帧布局描述: 供通用帧解析器定位关键字段(两个方向的帧头不同)
typedef struct {
  int size_min;            // 帧头+校验的字节数(不含payload)
  int payload_size_offset; // payload_size字段在帧内的字节偏移
  int payload_size_max;    // payload允许的最大字节数
} FMFrameLayout;
extern const FMFrameLayout fm_frame_dev_to_user_layout;
extern const FMFrameLayout fm_frame_user_to_dev_layout;

// 解析出一个完整帧后的回调(frame指向缓冲区内的一帧，长度为frame_size)
typedef void (*fm_frame_cb_f)(void *arg, const void *frame, int frame_size);

/**
 * @brief 通用帧解析: 将data写入缓冲区并尽可能多地提取完整帧
 *
 * 按SOF对齐并校验CRC，每解析出一帧调用cb; 已消费的数据会从缓冲区前移清除。
 *
 * @param buffer 帧缓冲区，由调用方持有(容量FM_FRAME_SIZE_MAX)
 * @param index_begin 有效数据起始游标，原地更新
 * @param index_end 有效数据结束游标，原地更新
 * @param layout 帧布局描述(两个方向的帧头不同)
 * @param data 新收到的数据
 * @param data_size data的字节数
 * @param cb 每提取到一帧时的回调
 * @param arg 原样透传给cb
 */
void fm_frame_buffer_feed(uint8_t *buffer, int *index_begin, int *index_end,
                          const FMFrameLayout *layout, const void *data,
                          int data_size, fm_frame_cb_f cb, void *arg);

#ifdef __cplusplus
}
#endif
