// 消息定义
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
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

// user表示用户侧，dev表示设备侧
// 消息方向：v:user>dev, ^:dev>user，
// 有消息ID但没有对应结构体定义的为空消息（payload_size为0）
// write消息和响应消息通常共用一个ID，结构体也共用
// 单个方向上，每个消息ID都是唯一的
enum {
  FM_MSG_INVALID,
  FM_MSG_ECHO = 3, // v^

  FM_MSG_FIND = 4, // v^

  FM_MSG_RESTART, // v^

  FM_MSG_PARAM_READ = 25,            // v
  FM_MSG_PARAM_WRITE,                // v
  FM_MSG_PARAM = FM_MSG_PARAM_WRITE, // ^

  FM_MSG_HEARTBEAT, // ^

  FM_MSG_RESULT = 29, // ^
  FM_MSG_PREV_RESULT, // ^

  FM_MSG_BEGIN_PAIR = 32, // v^
  FM_MSG_CANCEL_PAIR,     // v^

  FM_MSG_DATA_USER_TO_USER = 36, // v^

  FM_MSG_SPHERICAL_RESULT = 42, // ^
  FM_MSG_PREV_SPHERICAL_RESULT, // ^

  FM_MSG_DIS = 46, // ^

  FM_MSG_COUNT,
};

typedef int64_t fm_local_time_t;

// 测试双向通信，发送后原样返回
#define FM_DATA_ECHO_PAYLOAD_SIZE 64
typedef struct {
  uint8_t payload_size;
  uint8_t payload[FM_DATA_ECHO_PAYLOAD_SIZE];
} FMDataEcho;

// 用于寻找设备
typedef struct {
  uint8_t duration; // 设备收到后持续对外提示(如震动，闪灯)的时间
} FMDataFind;

// 用于重启设备，当设备配置参数变更后需要重启才会生效
typedef struct {
  // 延迟重启时间
  uint8_t delay;
  // 是否仅需要时重启
  bool only_when_need;
} FMDataRestart;

typedef struct {
  float z_expect;
  float z_expect_noise;
  float max_acceleration[3];
  uint8_t reserved;
  float min_dis;
  float min_freq_duration;
  uint8_t min_freq_count;
  uint8_t max_delta_rssi;
} FMDataParam;

#define FM_DATA_HARDWARE_NAME_SIZE 11
typedef struct {
  // 硬件信息
  struct {
    char name[FM_DATA_HARDWARE_NAME_SIZE];
    uint8_t version[2];
    uint8_t batch;
    uint8_t year;
    uint8_t month;
    uint8_t day;
  } hardware;
  // 固件信息
  struct {
    uint8_t series;
    uint8_t version[4];
  } firmware;
  // 下面为运行时信息
  // 表示当前是否有需要重启生效的参数变更
  bool need_restart;
  // 表示自动上次清零重启状态后发生了新的重启记录
  bool restart_info_dirty;
  // 本次复位后累计运行时间 s
  uint32_t uptime;
} FMDataHeartbeat;

// 定位结果
typedef struct {
  fm_local_time_t local_time; // us
  uint8_t cnt;
  float pos[3];
  float vel[3];
  float pos_noise[3];
  float vel_noise[3];
} FMDataResult;

// 基站在resp中发送给标签，以便标签也能输出定位结果
typedef struct {
  uint8_t cnt;
  float pos[3];
} FMDataPrevResult;

typedef struct {
  // 超过多长时间还未配对完成自动退出配对模式
  uint8_t timeout;
} FMDataBeginPair;

typedef struct {
  // 收到后就取消配对，超时后才允许建立新的配对
  uint8_t timeout;
} FMDataCancelPair;

// 通过uwb中转的user_data的最大负载长度
#define FM_USER_DATA_PAYLOAD_SIZE 78
typedef struct {
  uint8_t payload_size;
  uint8_t payload[FM_USER_DATA_PAYLOAD_SIZE];
} FMDataUserData;

typedef FMDataUserData FMDataDataUserToUser;

// 球坐标系结果，由Result转换而来
typedef struct {
  fm_local_time_t local_time; // us
  uint8_t cnt;
  float dis;
  float azimuth;
  float elevation;
} FMDataSphericalResult;

// 球坐标结果，由PrevResult转换而来
typedef struct {
  uint8_t cnt;
  float dis;
  float azimuth;
  float elevation;
} FMDataSphericalPrevResult;

typedef struct {
  fm_local_time_t local_time; // us
  uint8_t cnt;
  float dis;
  uint8_t rx_rate;
} FMDataDis;

#ifdef __cplusplus
}
#endif
