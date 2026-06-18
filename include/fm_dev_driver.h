// 用户需要使用的接口
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

// 数据帧最大长度
#define FM_FRAME_SIZE_MAX 255
// 设备UID长度
#define FM_UID_SIZE 6
// 数据帧计数
typedef uint8_t fm_frame_cnt_t;
// 消息id
typedef uint8_t fm_msg_id_t;

// 设备角色
typedef enum {
  FM_ANCHOR,
  FM_TAG,
} fm_role_e;

// 设备连接类型
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

  FM_MSG_DEBUG_READ,                 // v
  FM_MSG_DEBUG_WRITE,                // v
  FM_MSG_DEBUG = FM_MSG_DEBUG_WRITE, // ^

  FM_MSG_ECHO, // v^

  FM_MSG_FIND, // v^

  FM_MSG_RESTART, // v^

  FM_MSG_RESTART_INFO_READ = 13,                   // v
  FM_MSG_RESTART_INFO_CLEAR,                       // v 空消息
  FM_MSG_RESTART_INFO = FM_MSG_RESTART_INFO_CLEAR, // ^

  FM_MSG_ASSERT_INFO_READ,                       // v
  FM_MSG_ASSERT_INFO_CLEAR,                      // v 空消息
  FM_MSG_ASSERT_INFO = FM_MSG_ASSERT_INFO_CLEAR, // ^

  FM_MSG_PARAM_READ = 25,            // v
  FM_MSG_PARAM_WRITE,                // v
  FM_MSG_PARAM = FM_MSG_PARAM_WRITE, // ^

  FM_MSG_HEARTBEAT,         // ^
  FM_MSG_OUTSIDE_HEARTBEAT, // v

  FM_MSG_RESULT,      // ^
  FM_MSG_PREV_RESULT, // ^

  FM_MSG_BEGIN_PAIR = 32, // v^
  FM_MSG_CANCEL_PAIR,     // v^

  FM_MSG_DATA_USER_TO_DEV,  // v 用户不需要直接使用，内部接口会封装
  FM_MSG_DATA_DEV_TO_USER,  // ^ 用户不需要直接使用，内部接口会解析
  FM_MSG_DATA_USER_TO_USER, // v^

  FM_MSG_SPHERICAL_RESULT = 42, // ^
  FM_MSG_PREV_SPHERICAL_RESULT, // ^

  FM_MSG_DIS = 46, // ^

  FM_MSG_COUNT,
};

typedef int64_t fm_local_time_t;

// 开发调试用，测试信息读写，复位后丢失
#define FM_DATA_DEBUG_PAYLOAD_SIZE 64
typedef struct {
  uint8_t payload_size;
  uint8_t payload[FM_DATA_DEBUG_PAYLOAD_SIZE];
} FMDataDebug;

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

// 记录复位信息
#define FM_DATA_RESTART_INFO_INFO_SIZE 17
typedef struct {
  uint8_t undefined;
  uint8_t power_on;
  uint8_t pin;
  uint8_t watchdog;
  uint8_t software;
  uint8_t cpu_lockup;
  uint8_t wakeup;
  uint8_t reserved;
  fm_local_time_t local_time;                // 上次触发复位时的本地时间
  char info[FM_DATA_RESTART_INFO_INFO_SIZE]; // 复位信息
} FMDataRestartInfo;

// 记录上次断言信息
#define FM_DATA_ASSERT_INFO_INFO_SIZE 65
typedef struct {
  uint8_t assert_count;
  // 复位后短时间内就重启，连续触发一定次数将重置参数
  uint8_t assert_count_within_short_time;
  fm_local_time_t local_time;               // 上次触发断言时的本地时间
  char info[FM_DATA_ASSERT_INFO_INFO_SIZE]; // 断言信息
} FMDataAssertInfo;

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

typedef struct {
  bool is_backend;
  uint8_t timeout;
} FMDataOutsideHeartbeat;

// 定位结果，vel单位cm/s，noise单位cm
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

typedef FMDataUserData FMDataDataUserToDev;
typedef FMDataUserData FMDataDataDevToUser;
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

/**
 * @brief
 * 通过此函数将一个msg写入data封装为一帧数据，用户随后可直接将data发送给设备
 *
 * @param connect_type 连接类型(FM_WIRED:发送给有线直连的设备;
 * FM_WIRELESS:经过有线设备中转发送给无线侧的设备)
 * @param frame_cnt 帧计数，每次需要+1
 * @param msg_id 消息id(FM_MSG_*)
 * @param msg_payload 消息负载，需传入对应msg_id的结构体指针(FMData*)
 * @param frame 输出数据缓冲区
 * @param frame_size_max 输出数据缓冲区最大大小
 * @return int 返回封装好的数据帧大小
 */
int fm_prepare_msg_to_dev(fm_connect_type_e connect_type,
                          fm_frame_cnt_t frame_cnt, fm_msg_id_t msg_id,
                          const void *msg_payload, void *frame,
                          int frame_size_max);

// 以下为分步骤接口，适用于需要一帧数据中包含多个msg的情况
// 用法: begin 初始化 -> try_append 追加(可多次) -> end
// 收尾，frame为同一个缓冲区

/**
 * @brief 开始构建一帧发往设备的数据，初始化帧头(frame中的内容会被重置)
 *
 * @param frame_cnt 帧计数，每帧需要+1
 * @param frame 输出数据缓冲区(后续begin/try_append/end需传入同一缓冲)
 * @param frame_size_max 输出数据缓冲区最大大小
 */
void fm_prepare_msg_to_dev_begin(fm_frame_cnt_t frame_cnt, void *frame,
                                 int frame_size_max);
/**
 * @brief 向当前帧追加一个msg
 *
 * @param connect_type 连接类型(FM_WIRED:发送给有线直连的设备;
 * FM_WIRELESS:经过有线设备中转发送给无线侧的设备)
 * @param msg_id 消息id(FM_MSG_*)
 * @param msg_payload 消息负载，需传入对应msg_id的结构体指针(FMData*)
 * @param frame 输出数据缓冲区(同begin传入的缓冲)
 * @param frame_size_max 输出数据缓冲区最大大小
 * @return bool true:追加成功; false:剩余空间不足，未追加(应调用end结束当前帧)
 */
bool fm_prepare_msg_to_dev_try_append(fm_connect_type_e connect_type,
                                      fm_msg_id_t msg_id,
                                      const void *msg_payload, void *frame,
                                      int frame_size_max);
/**
 * @brief 结束当前帧构建，计算校验，随后可直接将frame发送给设备
 *
 * @param frame 输出数据缓冲区(同begin传入的缓冲)
 * @param frame_size_max 输出数据缓冲区最大大小
 * @return int 返回封装好的数据帧大小
 */
int fm_prepare_msg_to_dev_end(void *frame, int frame_size_max);

/**
 * @brief 收到消息的回调函数
 * @param connect_type 连接类型(FM_WIRED:消息来自有线直连的设备;
 * FM_WIRELESS:消息来自无线侧的设备)
 * @param role 消息所属设备的角色(FM_ANCHOR/FM_TAG)
 * @param wired_uid 有线直连节点的uid(长度FM_UID_SIZE)
 * @param frame_cnt 帧计数，每帧数据会自动+1
 * @param msg_id 消息id(FM_MSG_*)
 * @param msg_payload 消息负载，对应id的结构体指针(FMData*)
 * @param msg_payload_size 消息负载大小
 */
typedef void (*fm_on_msg_from_dev_f)(fm_connect_type_e connect_type,
                                     fm_role_e role, const uint8_t *wired_uid,
                                     fm_frame_cnt_t frame_cnt,
                                     fm_msg_id_t msg_id,
                                     const void *msg_payload,
                                     int msg_payload_size);
typedef struct {
  uint8_t buffer[FM_FRAME_SIZE_MAX];
  int index_begin;
  int index_end;
  fm_on_msg_from_dev_f on_msg;
  // 以下为处理当前帧时的瞬时状态
  fm_connect_type_e connect_type; // 当前msg的连接类型
  fm_role_e role;                 // 当前msg所属角色
  fm_role_e wired_role;           // 有线连接节点的角色
  fm_frame_cnt_t frame_cnt;       // 当前帧计数
  uint8_t wired_uid[FM_UID_SIZE]; // 有线连接节点的uid
} FMParserFromDev;
/**
 * @brief 初始化数据解析器
 *
 * @param parser 数据解析器实例
 * @param on_msg 消息回调函数
 */
void fm_parser_from_dev_init(FMParserFromDev *parser,
                             fm_on_msg_from_dev_f on_msg);
/**
 * @brief
 * 用指定解析器处理接收到的数据，内部会进行数据帧提取和消息解析，并调用on_msg回调函数
 *
 * @param parser 数据解析器实例
 * @param data 接收到的数据
 * @param data_size 数据大小
 */
void fm_parser_from_dev_handle_data(FMParserFromDev *parser, const void *data,
                                    int data_size);

/**
 * @brief 通过此函数将一个msg写入frame，设备端随后可直接将frame发送给用户
 *
 * @param role 设备自身角色(FM_ANCHOR/FM_TAG)
 * @param uid 设备自身uid(长度FM_UID_SIZE)，可传NULL表示全0
 * @param frame_cnt 帧计数，每次需要+1
 * @param msg_id 消息id(FM_MSG_*)
 * @param msg_payload 消息负载，需传入对应msg_id的结构体指针(FMData*)
 * @param frame 输出数据缓冲区
 * @param frame_size_max 输出数据缓冲区最大大小
 * @return int 返回写入的数据大小
 */
int fm_prepare_msg_to_user(fm_role_e role, const uint8_t *uid,
                           fm_frame_cnt_t frame_cnt, fm_msg_id_t msg_id,
                           const void *msg_payload, void *frame,
                           int frame_size_max);
// 以下为分步骤接口，适用于需要一帧数据中包含多个msg的情况
// 用法: begin 初始化 -> try_append 追加(可多次) -> end
// 收尾，frame为同一个缓冲区

/**
 * @brief 开始构建一帧发往用户的数据，初始化帧头(frame中的内容会被重置)
 *
 * @param role 设备自身角色(FM_ANCHOR/FM_TAG)
 * @param uid 设备自身uid(长度FM_UID_SIZE)，可传NULL表示全0
 * @param frame_cnt 帧计数，每帧需要+1
 * @param frame 输出数据缓冲区(后续begin/try_append/end需传入同一缓冲)
 * @param frame_size_max 输出数据缓冲区最大大小
 */
void fm_prepare_msg_to_user_begin(fm_role_e role, const uint8_t *uid,
                                  fm_frame_cnt_t frame_cnt, void *frame,
                                  int frame_size_max);
/**
 * @brief 向当前帧追加一个msg
 *
 * @param msg_id 消息id(FM_MSG_*)
 * @param msg_payload 消息负载，需传入对应msg_id的结构体指针(FMData*)
 * @param frame 输出数据缓冲区(同begin传入的缓冲)
 * @param frame_size_max 输出数据缓冲区最大大小
 * @return bool true:追加成功; false:剩余空间不足，未追加(应调用end结束当前帧)
 */
bool fm_prepare_msg_to_user_try_append(int msg_id, const void *msg_payload,
                                       void *frame, int frame_size_max);
/**
 * @brief 结束当前帧构建，计算校验，随后可直接将frame发送给用户
 *
 * @param frame 输出数据缓冲区(同begin传入的缓冲)
 * @param frame_size_max 输出数据缓冲区最大大小
 * @return int 返回封装好的数据帧大小
 */
int fm_prepare_msg_to_user_end(void *frame, int frame_size_max);

/**
 * @brief 收到消息的回调函数
 * @param frame_cnt 帧计数，每帧数据会自动+1
 * @param msg_id 消息id(FM_MSG_*)
 * @param msg_payload 消息负载，对应id的结构体指针(FMData*)
 * @param msg_payload_size 消息负载大小
 */
typedef void (*fm_on_msg_from_user_f)(fm_frame_cnt_t frame_cnt,
                                      fm_msg_id_t msg_id,
                                      const void *msg_payload,
                                      int msg_payload_size);
typedef struct {
  uint8_t buffer[FM_FRAME_SIZE_MAX];
  int index_begin;
  int index_end;
  fm_on_msg_from_user_f on_msg;
  // 以下为处理当前帧时的瞬时状态
  fm_frame_cnt_t frame_cnt; // 当前帧计数
} FMParserFromUser;
/**
 * @brief 初始化数据解析器
 *
 * @param parser 数据解析器实例
 * @param on_msg 消息回调函数
 */
void fm_parser_from_user_init(FMParserFromUser *parser,
                              fm_on_msg_from_user_f on_msg);
/**
 * @brief
 * 用指定解析器处理接收到的数据，内部会进行数据帧提取和消息解析，并调用on_msg回调函数
 *
 * @param parser 数据解析器实例
 * @param data 接收到的数据
 * @param data_size 数据大小
 */
void fm_parser_from_user_handle_data(FMParserFromUser *parser, const void *data,
                                     int data_size);

#ifdef __cplusplus
}
#endif
