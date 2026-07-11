// 定义消息格式及相关处理函数
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "fm_driver.h" // fm_msg_id_t
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// 单条消息最大长度
#define FM_MSG_SIZE_MAX 127
typedef uint8_t fm_msg_payload_size_t;
#define FM_MSG_HEADER_SIZE                                                     \
  ((int)(sizeof(fm_msg_id_t) + sizeof(fm_msg_payload_size_t)))
#define FM_MSG_PAYLOAD_SIZE_MAX (FM_MSG_SIZE_MAX - FM_MSG_HEADER_SIZE)

// 完整消息结构
#pragma pack(push, 1)
typedef struct {
  fm_msg_id_t id;
  fm_msg_payload_size_t payload_size;
  uint8_t payload[FM_MSG_PAYLOAD_SIZE_MAX];
} FMData;
#pragma pack(pop)

static inline int fm_msg_size(const FMData *msg) {
  return FM_MSG_HEADER_SIZE + msg->payload_size;
}

// 将一个msg(id+payload)写入dst，返回写入的总字节数
static inline int fm_msg_write(void *dst, fm_msg_id_t id, const void *payload,
                               int payload_size) {
  FMData *msg = (FMData *)dst;
  msg->id = id;
  msg->payload_size = (fm_msg_payload_size_t)payload_size;
  memcpy(msg->payload, payload, payload_size);
  return FM_MSG_HEADER_SIZE + payload_size;
}

// 以版本兼容方式拷贝payload到目标结构体: 多则截断，少则保留0
static inline void fm_msg_copy_payload(void *dst, int dst_size,
                                       const void *payload, int payload_size) {
  int valid_size = payload_size <= dst_size ? payload_size : dst_size;
  memset(dst, 0, dst_size);
  memcpy(dst, payload, valid_size);
}

// 逐个解析data中的msg，对每个msg调用cb
typedef void (*fm_msg_cb_f)(void *arg, const FMData *msg);
static inline void fm_msg_parser_handle_data(fm_msg_cb_f cb, void *arg,
                                             const void *data, int data_size) {
  int addr = 0;
  const uint8_t *p = (const uint8_t *)data;
  while (addr + FM_MSG_HEADER_SIZE <= data_size) {
    const FMData *msg = (const FMData *)(p + addr);
    int msg_size = fm_msg_size(msg);
    if (addr + msg_size > data_size) {
      break;
    }
    cb(arg, msg);
    addr += msg_size;
  }
}

#ifdef __cplusplus
}
#endif
