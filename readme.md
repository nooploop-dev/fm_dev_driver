# AOA DEVICE DRIVER

简体中文 | [English](./readme.en.md)

本仓库为 Nooploop [AOA Follow Me](https://www.nooploop.com/cn/follow-me/) 产品的驱动代码。它提供：

- **纯 C 驱动**（`include/` + `src/`）：无任何第三方依赖，可直接集成进单片机/嵌入式工程，负责设备通信协议的「组包」与「解析」。
- **ROS1 / ROS2 驱动包**（`app/` + `msg/` + `launch/`）：在纯 C 驱动之上封装串口收发与话题转换，clone 到 ROS 工作空间即可使用。

## 目录结构

```text
fm_driver/
├── include/                  # 对外公共头
│   ├── fm_driver_for_user.h  #   用户端接口(集成时通常只需这一个)
│   └── fm_driver_data.h      #   消息ID与消息结构体定义(被上面的头包含)
├── src/                      # 纯C驱动实现(需一起加入编译)
│   ├── fm_crc.c/.h           #   CRC 校验
│   ├── fm_frame.c/.h         #   帧组装/拆解
│   ├── fm_driver.c           #   组包/解析主逻辑
│   ├── fm_driver_raw.h       #   协议层结构体与转换(内部)
│   └── fm_msg.h              #   消息层(内部)
├── test/                     # 接口用法的完整示例(强烈建议参考)
├── app/                      # 上位机应用与集成示例
│   ├── reader/               #   从串口接收并解析(含可选 Foxglove 可视化)
│   ├── writer/               #   组包并通过串口向设备发送
│   ├── ros1_converter/       #   ROS1 话题转换节点
│   └── ros2_converter/       #   ROS2 话题转换节点
├── msg/                      # ROS 消息定义
├── launch/                   # ROS launch 文件
├── rviz/                     # rviz 显示配置(ROS1/ROS2 各一份)
├── package.xml               # ROS 包描述
└── CMakeLists.txt
```

## 纯 C 驱动集成（单片机 / 嵌入式）

纯 C 驱动只做两件事：把要发给设备的消息**组装成数据帧**，以及把从设备收到的字节流**解析成消息**。数据的实际收发由用户自行实现。

### 加入工程

- 源文件：`src/fm_crc.c`、`src/fm_frame.c`、`src/fm_driver.c`
- 头文件目录：`include/`

业务代码中只需 `#include "fm_driver_for_user.h"`。驱动遵循 C11，无动态内存分配、无第三方依赖，适合资源受限的 MCU。

> 纯 C 工程不需要 `app/`。若想了解如何把驱动接口接入实际收发流程，可参考 [`app/reader/`](app/reader/)（解析）和 [`app/writer/`](app/writer/)（组包）。示例用 C++ 实现串口与日志，但调用的驱动接口与纯 C 工程完全相同。

### 接口概览

接口定义在 [include/fm_driver_for_user.h](include/fm_driver_for_user.h)，消息 ID 与消息结构体定义在 [include/fm_driver_data.h](include/fm_driver_data.h)（前者已包含后者，业务代码只需 include 前者）。通信分「设备 → 用户」和「用户 → 设备」两个方向：

- **组包**：`fm_prepare_msg_to_dev()` 将一个消息（`FMData*` 结构体）封装为一帧，返回帧长度，随后把该帧通过串口发给设备。
  - 如需在一帧里打包多条消息，使用分步接口：`fm_prepare_msg_to_dev_begin()` → `fm_prepare_msg_to_dev_try_append()`（可多次）→ `fm_prepare_msg_to_dev_end()`。
- **解析**：用 `FMParserFromDev` + `fm_parser_from_dev_init()` 注册回调，串口每收到一段数据就喂给 `fm_parser_from_dev_handle_data()`，内部自动完成拆帧与解析。每帧开始时回调 `on_frame_begin`（提供角色/uid/帧计数等帧头信息），帧内每解析出一条消息回调一次 `on_frame_msg`（按 `msg_id` 取对应的 `FMData*` 结构体），处理完一帧后回调 `on_frame_end`；不需要的回调可传 `NULL`。

> 消息 ID（`FM_MSG_*`）与结构体（`FMData*`）的一一映射、各字段含义与单位，均见 [include/fm_driver_data.h](include/fm_driver_data.h) 的注释。
>
> 若你开发的是**设备端固件**（需要组包发给用户），接口见 [include/fm_driver_for_dev.h](include/fm_driver_for_dev.h)。

### 最小示例

```c
#include "fm_driver_for_user.h"
#include <stdio.h>

// —— 接收：每解析出一条设备上报的消息时被回调 ——
// connect_type 为 FM_WIRED 表示消息来自有线直连的设备，
// FM_WIRELESS 表示来自无线侧的设备
static void on_frame_msg_from_dev(fm_connect_type_e connect_type,
                                  fm_msg_id_t msg_id, const void *msg_payload,
                                  int msg_payload_size) {
  switch (msg_id) {
  case FM_MSG_SPHERICAL_RESULT: { // 球坐标定位结果
    const FMDataSphericalResult *r =
        (const FMDataSphericalResult *)msg_payload;
    printf("dis=%.2f azimuth=%.2f elevation=%.2f\n", r->dis, r->azimuth,
           r->elevation);
    break;
  }
  default:
    break;
  }
}

static FMParserFromDev g_parser;

void app_init(void) {
  // 如需帧头信息(角色/uid/帧计数)，注册 on_frame_begin/on_frame_end 回调
  fm_parser_from_dev_init(&g_parser, NULL, on_frame_msg_from_dev, NULL);
}

// 串口中断/轮询收到数据后调用
void app_on_serial_rx(const uint8_t *data, int size) {
  fm_parser_from_dev_handle_data(&g_parser, data, size);
}

// —— 发送：向设备下发一条命令(例如让设备震动/闪灯提示5秒) ——
void app_send_find(void) {
  FMDataFind find = {0};
  find.duration = 5;

  uint8_t frame[FM_FRAME_SIZE_MAX];
  static fm_frame_cnt_t cnt = 0; // 帧计数，每帧 +1
  int n = fm_prepare_msg_to_dev(FM_WIRED, cnt++, FM_MSG_FIND, &find, frame,
                                sizeof(frame));
  // user_serial_write 为用户自行实现的串口发送函数
  user_serial_write(frame, n);
}
```

### 更多用法

[test/test_fm_driver.cpp](test/test_fm_driver.cpp) 覆盖了**所有消息**在两个方向上的组包与解析往返用例，是最完整、最权威的接口用法参考，集成时可直接对照。

## ROS 集成

ROS1 与 ROS2 共用同一份代码，节点名与可执行文件名均为 `ros_converter`。构建时依据环境变量 `ROS_VERSION` 自动选择版本，无需手动指定。

> 前置：已安装 ROS 并完成 `source /opt/ros/<distro>/setup.bash`。

### 编译

把本仓库 clone 到工作空间的 `src/` 下（目录名为 `fm_driver`），然后：

| | ROS1 | ROS2 |
| --- | --- | --- |
| 编译 | `catkin_make` | `colcon build` |
| 环境 | `source devel/setup.bash` | `source install/setup.bash` |

### 运行

把 AOA 有线设备通过串口接入电脑，确认串口号后启动：

```bash
roslaunch fm_driver msg.launch port:=/dev/ttyUSB0     # ROS1
ros2 launch fm_driver msg.py   port:=/dev/ttyACM0     # ROS2
```

两者通用的参数：

- `port`：串口设备号
- `baudrate`：波特率，默认 `921600`
- `frame_id`：定位结果所在坐标系，写入 `header.frame_id`，默认 `fm_anchor_link`

ROS2 也可不用 launch 直接运行节点：

```bash
ros2 run fm_driver ros_converter --ros-args -p port:=/dev/ttyACM0
```

> 若启动时报串口「Permission denied」，见下文 [串口权限（Linux）](#串口权限linux)。

### 可视化（rviz）

用 `rviz` 这套 launch 一步启动「驱动 + 静态 TF + rviz」，开箱即可看到定位结果（位置点 + 不确定性椭球）：

```bash
roslaunch fm_driver rviz.launch port:=/dev/ttyUSB0    # ROS1
ros2 launch fm_driver rviz.py   port:=/dev/ttyACM0    # ROS2
```

除上面的参数外，还支持：

- `parent_frame`：基站坐标系的父坐标系，默认 `map`
- 基站在 `parent_frame` 中的安装位姿：ROS1 用 `anchor_pose:="x y z yaw pitch roll"`（默认全 0）；ROS2 用 `x`/`y`/`z`/`yaw`/`pitch`/`roll` 分别指定
- `static_tf`：是否发布 `parent_frame` → `frame_id` 的静态 TF，默认 `true`。若该 TF 已由其它节点（如机器人 URDF）发布，置 `false` 避免冲突

> 定位结果发布在基站自身坐标系 `fm_anchor_link` 下，基站的安装位姿属于 URDF/标定的职责，驱动本身不发布 TF。上面的静态 TF 只是为了让裸测时 rviz 有一棵可用的 TF 树。

### 查看数据

```bash
rostopic echo /ros_converter/result       # ROS1
ros2 topic echo /ros_converter/result     # ROS2
```

### 话题

话题均以节点名为命名空间，即 `/ros_converter/<话题名>`。消息字段定义见 [msg/](msg/) 目录。

**设备 → 用户**（订阅以获取数据）

| 话题 | 消息类型 | 说明 |
| --- | --- | --- |
| `~/result` | `Result` | 定位结果（位置/速度/噪声） |
| `~/result_pose` | `geometry_msgs/PoseWithCovarianceStamped` | 同一份定位结果的标准消息，供 rviz 直接显示 |
| `~/prev_result` | `PrevResult` | 上一次定位结果（TAG） |
| `~/spherical_result` | `SphericalResult` | 球坐标定位结果（距离/方位角/俯仰角） |
| `~/prev_spherical_result` | `PrevSphericalResult` | 上一次球坐标结果（TAG） |
| `~/dis` | `Dis` | 测距结果 |
| `~/heartbeat` | `Heartbeat` | 设备心跳/状态信息 |
| `~/param` | `Param` | 参数读取响应/当前参数 |
| `~/echo_from_device` | `Echo` | 设备回显 |
| `~/user_data_from_device` | `DataUserToUser` | 设备转发的用户自定义数据 |

**用户 → 设备**（发布以下发命令）

| 话题 | 消息类型 | 说明 |
| --- | --- | --- |
| `~/find` | `Find` | 让设备震动/闪灯以便寻找 |
| `~/restart` | `Restart` | 重启设备 |
| `~/param_read` | `std_msgs/Empty` | 读取设备参数 |
| `~/param_write` | `Param` | 写入设备参数 |
| `~/begin_pair` | `BeginPair` | 进入配对模式 |
| `~/cancel_pair` | `CancelPair` | 取消配对 |
| `~/echo_to_device` | `Echo` | 回显测试 |
| `~/user_data_to_device` | `DataUserToUser` | 下发用户自定义数据 |

## Foxglove 可视化（无需 ROS）

`app/reader/` 在原有日志打印之外，可内置一个 Foxglove WebSocket 服务，不依赖 ROS，适合在没有 ROS 环境的机器上直接看数据。**非 ROS 构建下默认开启**：

```bash
cmake -B build && cmake --build build
./build/app/reader/reader --port /dev/ttyUSB0   # 波特率默认 921600，Foxglove 端口默认 8765
./build/app/reader/reader --port /dev/ttyUSB0 --baudrate 921600 --foxglove_port 8765
```

然后在 Foxglove 中选择 **Open connection → Foxglove WebSocket**，地址填 `ws://<设备IP>:8765`。日志打印保持原样，两者同时输出。

| 话题 | 类型 | 用途 |
| --- | --- | --- |
| `/result_pose` | `foxglove.PoseInFrame` | 定位点，3D 面板显示 |
| `/result_scene` | `foxglove.SceneUpdate` | 不确定性球（直径 2σ）+ 速度矢量线段 |
| `/tf` | `foxglove.FrameTransform` | `world` → `fm_anchor_link`，3D 面板的坐标系 |
| `/result` | JSON | 位置/速度/噪声，供 Plot 面板绘制曲线 |
| `/spherical_result` | JSON | 距离/方位角/俯仰角，供 Plot 面板绘制曲线 |
| `/dis` | JSON | 测距结果，供 Plot 面板绘制曲线 |

### 数据录制（MCAP）

加 `--record` 即可把上表中的所有话题录制成 MCAP 文件，事后可直接拖进 Foxglove 回放：

```bash
./build/app/reader/reader --port /dev/ttyUSB0 --record
```

文件写入 `logs/fm_<日期时间>.mcap`（与日志同目录，例如 `logs/fm_20260713_182432_123.mcap`，精确到毫秒，多次运行不会互相覆盖）。录制与 WebSocket 服务相互独立，可同时使用。

> 录制依赖 Foxglove SDK，需要 `FM_BUILD_FOXGLOVE=ON`（非 ROS 构建下默认开启）。请用 `Ctrl+C` 正常退出，程序会在退出时写入索引并关闭文件；强杀（`kill -9`）会导致 MCAP 缺少索引而无法打开。

## 串口权限（Linux）

首次访问串口时常见如下报错：

```text
could not open port /dev/ttyUSB0: [Errno 13] Permission denied: '/dev/ttyUSB0'
```

原因是串口设备属于 `dialout` 组（部分发行版为 `uucp`），而普通用户默认不在该组。用 `ls -l /dev/ttyUSB0` 确认组名后，把当前用户加入该组，**重新登录**生效：

```bash
sudo usermod -a -G dialout $USER   # -a 不可省略，否则会把用户移出其它附加组
```

参考：[Fix Serial Port “Permission Denied” Errors on Linux](https://websistent.com/fix-serial-port-permission-denied-errors-linux/)

## 构建选项（CMake）

| 选项 | 默认 | 说明 |
| --- | --- | --- |
| `FM_BUILD_ROS1` | 依 `ROS_VERSION` 自动检测 | 构建 ROS1 驱动包 |
| `FM_BUILD_ROS2` | 依 `ROS_VERSION` 自动检测 | 构建 ROS2 驱动包 |
| `FM_BUILD_READER` | 非 ROS 构建 ON / ROS 构建 OFF | 构建串口接收与解析示例 |
| `FM_BUILD_WRITER` | 非 ROS 构建 ON / ROS 构建 OFF | 构建组包与串口发送示例 |
| `FM_BUILD_FOXGLOVE` | 非 ROS 构建 ON / ROS 构建 OFF | 为 reader 启用 Foxglove 可视化（自动拉取 Foxglove SDK） |
| `FM_BUILD_TEST` | 非 ROS 构建 ON / ROS 构建 OFF | 构建纯 C 驱动单元测试（自动拉取 Catch2） |

未显式指定 ROS 选项时，会依据环境变量 `ROS_VERSION` 自动选择，因此在 ROS 工作空间中直接用 `catkin_make` / `colcon build` 即可。

一旦启用 ROS1 或 ROS2，后四个选项默认全部关闭：ROS 构建只产出 `ros_converter` 节点，不会连带编译上位机示例、单元测试，也不会去拉取 Catch2 / Foxglove SDK。如仍需其中某项，显式打开即可，例如在 ROS 工作空间里编译并运行单元测试：

```bash
catkin_make -DFM_BUILD_TEST=ON                              # ROS1
colcon build --cmake-args -DFM_BUILD_TEST=ON                # ROS2
```
