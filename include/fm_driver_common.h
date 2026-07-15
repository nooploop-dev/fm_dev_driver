#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define FM_FRAME_SIZE_MAX 255
#define FM_UID_SIZE 6
typedef uint8_t fm_frame_cnt_t;
typedef uint8_t fm_msg_id_t;

// 设备角色
typedef enum {
  FM_ANCHOR,
  FM_TAG,
} fm_role_e;

// 设备连接类型，可通过直连设备中转，向无线设备收发消息
typedef enum {
  FM_WIRELESS,
  FM_WIRED,
} fm_connect_type_e;

#ifdef __cplusplus
}
#endif
