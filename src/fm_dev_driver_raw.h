// 定义原始消息格式及和用户侧消息的双向转换
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "fm_dev_driver.h"
#include "fm_dev_msg.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static inline int16_t fm_clamp_cast_i16(float v) {
  if (v > 32767.0f)
    v = 32767.0f;
  else if (v < -32768.0f)
    v = -32768.0f;
  return (int16_t)v;
}
static inline uint16_t fm_clamp_cast_u16(float v) {
  if (v > 65535.0f)
    v = 65535.0f;
  else if (v < 0.0f)
    v = 0.0f;
  return (uint16_t)v;
}

// 计算payload形式的数据的字节数
#define FM_PAYLOAD_VALID_SIZE(data)                                            \
  (int)((const uint8_t *)&(data).payload - (const uint8_t *)&(data) +          \
        (data).payload_size)

#pragma pack(push, 1)

typedef struct {
  uint8_t payload_size;
  uint8_t payload[FM_DATA_ECHO_PAYLOAD_SIZE];
} FMRawDataEcho;
static inline void fm_data_echo_from_raw(const FMRawDataEcho *raw_data,
                                         FMDataEcho *data) {
  data->payload_size = raw_data->payload_size;
  memcpy(data->payload, raw_data->payload, raw_data->payload_size);
}
static inline void fm_data_echo_to_raw(const FMDataEcho *data, void *raw_data,
                                       int *raw_data_size) {
  FMRawDataEcho *raw = (FMRawDataEcho *)raw_data;
  memset(raw, 0, sizeof(*raw));
  raw->payload_size = data->payload_size;
  memcpy(raw->payload, data->payload, data->payload_size);
  *raw_data_size = FM_PAYLOAD_VALID_SIZE(*raw);
}

typedef struct {
  uint8_t duration;
} FMRawDataFind;
static inline void fm_data_find_from_raw(const FMRawDataFind *raw_data,
                                         FMDataFind *data) {
  data->duration = raw_data->duration;
}
static inline void fm_data_find_to_raw(const FMDataFind *data, void *raw_data,
                                       int *raw_data_size) {
  FMRawDataFind *raw = (FMRawDataFind *)raw_data;
  memset(raw, 0, sizeof(*raw));
  raw->duration = data->duration;
  *raw_data_size = sizeof(*raw);
}

typedef struct {
  uint8_t delay;
  uint8_t only_when_need : 1;
} FMRawDataRestart;
static inline void fm_data_restart_from_raw(const FMRawDataRestart *raw_data,
                                            FMDataRestart *data) {
  data->delay = raw_data->delay;
  data->only_when_need = raw_data->only_when_need;
}
static inline void fm_data_restart_to_raw(const FMDataRestart *data,
                                          void *raw_data, int *raw_data_size) {
  FMRawDataRestart *raw = (FMRawDataRestart *)raw_data;
  memset(raw, 0, sizeof(*raw));
  raw->delay = data->delay;
  raw->only_when_need = data->only_when_need;
  *raw_data_size = sizeof(*raw);
}

typedef struct {
  float z_expect;
  float z_expect_noise;
  float max_acceleration[3];
  uint8_t reserved : 1;
  uint8_t : 0;
  float min_dis;
  float min_freq_duration;
  uint8_t min_freq_count;
  uint8_t max_delta_rssi;
} FMRawDataParam;
static inline void fm_data_param_from_raw(const FMRawDataParam *raw_data,
                                          FMDataParam *data) {
  data->reserved = raw_data->reserved;
  data->z_expect = raw_data->z_expect;
  data->z_expect_noise = raw_data->z_expect_noise;
  for (size_t i = 0; i < 3; i++) {
    data->max_acceleration[i] = raw_data->max_acceleration[i];
  }
  data->min_dis = raw_data->min_dis;
  data->min_freq_duration = raw_data->min_freq_duration;
  data->min_freq_count = raw_data->min_freq_count;
  data->max_delta_rssi = raw_data->max_delta_rssi;
}
static inline void fm_data_param_to_raw(const FMDataParam *data, void *raw_data,
                                        int *raw_data_size) {
  FMRawDataParam *raw = (FMRawDataParam *)raw_data;
  memset(raw, 0, sizeof(*raw));
  raw->reserved = data->reserved;
  raw->z_expect = data->z_expect;
  raw->z_expect_noise = data->z_expect_noise;
  for (size_t i = 0; i < 3; i++) {
    raw->max_acceleration[i] = data->max_acceleration[i];
  }
  raw->min_dis = data->min_dis;
  raw->min_freq_duration = data->min_freq_duration;
  raw->min_freq_count = data->min_freq_count;
  raw->max_delta_rssi = data->max_delta_rssi;
  *raw_data_size = sizeof(*raw);
}

typedef struct {
  struct {
    char name[FM_DATA_HARDWARE_NAME_SIZE];
    uint8_t version[2];
    uint8_t batch;
    uint16_t year : 7;
    uint16_t month : 4;
    uint16_t day : 5;
  } hardware;
  struct {
    uint8_t series;
    uint8_t version[4];
  } firmware;
  uint8_t need_restart : 1;
  uint8_t restart_info_dirty : 1;
  uint8_t : 0;
  uint32_t uptime;
} FMRawDataHeartbeat;
static inline void
fm_data_heartbeat_from_raw(const FMRawDataHeartbeat *raw_data,
                           FMDataHeartbeat *data) {
  size_t len =
      strnlen(raw_data->hardware.name, sizeof(raw_data->hardware.name));
  memcpy(data->hardware.name, raw_data->hardware.name, len);
  data->hardware.name[len] = '\0';
  data->hardware.version[0] = raw_data->hardware.version[0];
  data->hardware.version[1] = raw_data->hardware.version[1];
  data->hardware.batch = raw_data->hardware.batch;
  data->hardware.year = (uint8_t)raw_data->hardware.year;
  data->hardware.month = (uint8_t)raw_data->hardware.month;
  data->hardware.day = (uint8_t)raw_data->hardware.day;
  data->firmware.series = raw_data->firmware.series;
  data->firmware.version[0] = raw_data->firmware.version[0];
  data->firmware.version[1] = raw_data->firmware.version[1];
  data->firmware.version[2] = raw_data->firmware.version[2];
  data->firmware.version[3] = raw_data->firmware.version[3];
  data->need_restart = raw_data->need_restart;
  data->restart_info_dirty = raw_data->restart_info_dirty;
  data->uptime = raw_data->uptime;
}
static inline void fm_data_heartbeat_to_raw(const FMDataHeartbeat *data,
                                            void *raw_data,
                                            int *raw_data_size) {
  FMRawDataHeartbeat *raw = (FMRawDataHeartbeat *)raw_data;
  memset(raw, 0, sizeof(*raw));
  size_t size = strnlen(data->hardware.name, sizeof(raw->hardware.name) - 1);
  memcpy(raw->hardware.name, data->hardware.name, size);
  raw->hardware.name[size] = '\0';
  raw->hardware.version[0] = data->hardware.version[0];
  raw->hardware.version[1] = data->hardware.version[1];
  raw->hardware.batch = data->hardware.batch;
  raw->hardware.year = data->hardware.year;
  raw->hardware.month = data->hardware.month;
  raw->hardware.day = data->hardware.day;
  raw->firmware.series = data->firmware.series;
  for (size_t i = 0; i < sizeof(raw->firmware.version); i++) {
    raw->firmware.version[i] = data->firmware.version[i];
  }
  raw->need_restart = data->need_restart;
  raw->restart_info_dirty = data->restart_info_dirty;
  raw->uptime = data->uptime;
  *raw_data_size = sizeof(*raw);
}

typedef struct {
  uint64_t local_time : 56;
  uint64_t cnt : 8;
  float pos[3];
  int16_t vel[3];        // vel = vel/100
  uint16_t pos_noise[3]; // pos_noise = pos_noise/100
  uint16_t vel_noise[3]; // vel_noise = vel_noise/100
} FMRawDataResult;
static inline void fm_data_result_from_raw(const FMRawDataResult *raw_data,
                                           FMDataResult *data) {
  data->local_time = (int64_t)raw_data->local_time;
  data->cnt = (uint8_t)raw_data->cnt;
  for (int i = 0; i < 3; i++) {
    data->pos[i] = raw_data->pos[i];
    data->vel[i] = raw_data->vel[i] / 100.0f;
    data->pos_noise[i] = raw_data->pos_noise[i] / 100.0f;
    data->vel_noise[i] = raw_data->vel_noise[i] / 100.0f;
  }
}
static inline void fm_data_result_to_raw(const FMDataResult *data,
                                         void *raw_data, int *raw_data_size) {
  FMRawDataResult *raw = (FMRawDataResult *)raw_data;
  memset(raw, 0, sizeof(*raw));
  raw->local_time = (uint64_t)data->local_time;
  raw->cnt = data->cnt;
  for (int i = 0; i < 3; i++) {
    raw->pos[i] = data->pos[i];
    raw->vel[i] = fm_clamp_cast_i16(data->vel[i] * 100.0f);
    raw->pos_noise[i] = fm_clamp_cast_u16(data->pos_noise[i] * 100.0f);
    raw->vel_noise[i] = fm_clamp_cast_u16(data->vel_noise[i] * 100.0f);
  }
  *raw_data_size = sizeof(*raw);
}

typedef struct {
  uint8_t cnt;
  float pos[3];
} FMRawDataPrevResult;
static inline void
fm_data_prev_result_from_raw(const FMRawDataPrevResult *raw_data,
                             FMDataPrevResult *data) {
  data->cnt = raw_data->cnt;
  data->pos[0] = raw_data->pos[0];
  data->pos[1] = raw_data->pos[1];
  data->pos[2] = raw_data->pos[2];
}
static inline void fm_data_prev_result_to_raw(const FMDataPrevResult *data,
                                              void *raw_data,
                                              int *raw_data_size) {
  FMRawDataPrevResult *raw = (FMRawDataPrevResult *)raw_data;
  memset(raw, 0, sizeof(*raw));
  raw->cnt = data->cnt;
  raw->pos[0] = data->pos[0];
  raw->pos[1] = data->pos[1];
  raw->pos[2] = data->pos[2];
  *raw_data_size = sizeof(*raw);
}

typedef struct {
  uint64_t local_time : 56;
  uint64_t cnt : 8;
  float dis;
  float azimuth;
  float elevation;
} FMRawDataSphericalResult;
static inline void
fm_data_spherical_result_from_raw(const FMRawDataSphericalResult *raw_data,
                                  FMDataSphericalResult *data) {
  data->local_time = (int64_t)raw_data->local_time;
  data->cnt = (uint8_t)raw_data->cnt;
  data->dis = raw_data->dis;
  data->azimuth = raw_data->azimuth;
  data->elevation = raw_data->elevation;
}
static inline void
fm_data_spherical_result_to_raw(const FMDataSphericalResult *data,
                                void *raw_data, int *raw_data_size) {
  FMRawDataSphericalResult *raw = (FMRawDataSphericalResult *)raw_data;
  memset(raw, 0, sizeof(*raw));
  raw->local_time = (uint64_t)data->local_time;
  raw->cnt = data->cnt;
  raw->dis = data->dis;
  raw->azimuth = data->azimuth;
  raw->elevation = data->elevation;
  *raw_data_size = sizeof(*raw);
}

typedef struct {
  uint8_t cnt;
  float dis;
  float azimuth;
  float elevation;
} FMRawDataPrevSphericalResult;
static inline void fm_data_spherical_prev_result_from_raw(
    const FMRawDataPrevSphericalResult *raw_data,
    FMDataSphericalPrevResult *data) {
  data->cnt = raw_data->cnt;
  data->dis = raw_data->dis;
  data->azimuth = raw_data->azimuth;
  data->elevation = raw_data->elevation;
}
static inline void
fm_data_spherical_prev_result_to_raw(const FMDataSphericalPrevResult *data,
                                     void *raw_data, int *raw_data_size) {
  FMRawDataPrevSphericalResult *raw = (FMRawDataPrevSphericalResult *)raw_data;
  memset(raw, 0, sizeof(*raw));
  raw->cnt = data->cnt;
  raw->dis = data->dis;
  raw->azimuth = data->azimuth;
  raw->elevation = data->elevation;
  *raw_data_size = sizeof(*raw);
}

typedef struct {
  uint64_t local_time : 56;
  uint64_t cnt : 8;
  float dis;
  uint8_t rx_rate;
} FMRawDataDis;
static inline void fm_data_dis_from_raw(const FMRawDataDis *raw_data,
                                        FMDataDis *data) {
  data->local_time = raw_data->local_time;
  data->cnt = (uint8_t)raw_data->cnt;
  data->dis = raw_data->dis;
  data->rx_rate = raw_data->rx_rate;
}
static inline void fm_data_dis_to_raw(const FMDataDis *data, void *raw_data,
                                      int *raw_data_size) {
  FMRawDataDis *raw = (FMRawDataDis *)raw_data;
  memset(raw, 0, sizeof(*raw));
  raw->local_time = data->local_time;
  raw->cnt = data->cnt;
  raw->dis = data->dis;
  raw->rx_rate = data->rx_rate;
  *raw_data_size = sizeof(*raw);
}

typedef struct {
  uint8_t timeout;
} FMRawDataBeginPair;
static inline void
fm_data_begin_pair_from_raw(const FMRawDataBeginPair *raw_data,
                            FMDataBeginPair *data) {
  data->timeout = raw_data->timeout;
}
static inline void fm_data_begin_pair_to_raw(const FMDataBeginPair *data,
                                             void *raw_data,
                                             int *raw_data_size) {
  FMRawDataBeginPair *raw = (FMRawDataBeginPair *)raw_data;
  memset(raw, 0, sizeof(*raw));
  raw->timeout = data->timeout;
  *raw_data_size = sizeof(*raw);
}

typedef struct {
  uint8_t timeout;
} FMRawDataCancelPair;
static inline void
fm_data_cancel_pair_from_raw(const FMRawDataCancelPair *raw_data,
                             FMDataCancelPair *data) {
  data->timeout = raw_data->timeout;
}
static inline void fm_data_cancel_pair_to_raw(const FMDataCancelPair *data,
                                              void *raw_data,
                                              int *raw_data_size) {
  FMRawDataCancelPair *raw = (FMRawDataCancelPair *)raw_data;
  memset(raw, 0, sizeof(*raw));
  raw->timeout = data->timeout;
  *raw_data_size = sizeof(*raw);
}

typedef struct {
  uint8_t payload_size;
  uint8_t payload[FM_USER_DATA_PAYLOAD_SIZE];
} FMRawDataUserData;
typedef FMRawDataUserData FMRawDataDataUserToDev;
typedef FMRawDataUserData FMRawDataDataDevToUser;
typedef FMRawDataUserData FMRawDataDataUserToUser;

static inline void
fm_data_user_to_user_from_raw(const FMRawDataDataUserToUser *raw_data,
                              FMDataDataUserToUser *data) {
  data->payload_size = raw_data->payload_size;
  memcpy(data->payload, raw_data->payload, raw_data->payload_size);
}
static inline void fm_data_user_to_user_to_raw(const FMDataDataUserToUser *data,
                                               void *raw_data,
                                               int *raw_data_size) {
  FMRawDataDataUserToUser *raw = (FMRawDataDataUserToUser *)raw_data;
  memset(raw, 0, sizeof(*raw));
  raw->payload_size = data->payload_size;
  memcpy(raw->payload, data->payload, data->payload_size);
  *raw_data_size = FM_PAYLOAD_VALID_SIZE(*raw);
}

#pragma pack(pop)

#ifdef __cplusplus
}
#endif
