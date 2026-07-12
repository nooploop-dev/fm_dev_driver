# AOA DEVICE DRIVER

简体中文 | [English](./readme.en.md)

本仓库为 Nooploop [AOA Follow Me](https://www.nooploop.com/cn/follow-me/) 产品的驱动代码。它提供：

- **纯 C 驱动**（`include/` + `src/`）：无任何第三方依赖，可直接集成进单片机/嵌入式工程，负责设备通信协议的「组包」与「解析」。
- **ROS1 / ROS2 驱动包**（`app/` + `msg/` + `launch/`）：在纯 C 驱动之上封装串口收发与话题转换，clone 到 ROS 工作空间即可使用。

## 目录结构

```text
fm_driver/
├── include/fm_driver.h   # 对外唯一公共头(用户只需关心这个文件)
├── src/                      # 纯C驱动实现(需一起加入编译)
│   ├── fm_crc.c/.h       #   CRC 校验
│   ├── fm_frame.c/.h     #   帧组装/拆解
│   ├── fm_driver.c       #   组包/解析主逻辑
│   ├── fm_driver_raw.h   #   协议层结构体与转换(内部)
│   └── fm_msg.h          #   消息层(内部)
├── test/test_fm_driver.cpp  # 接口用法的完整示例(强烈建议参考)
├── app/                      # 上位机应用与集成示例
│   ├── reader/               # 示例：从串口接收并解析设备数据
│   ├── writer/               # 示例：组包并通过串口向设备发送数据
│   ├── ros1_converter/       # ROS1 话题与设备数据转换节点
│   └── ros2_converter/       # ROS2 话题与设备数据转换节点
├── msg/                      # ROS 消息定义
├── launch/                   # ROS launch 文件
├── rviz/                     # rviz 显示配置(ROS1/ROS2 各一份)
├── package.xml               # ROS 包描述
└── CMakeLists.txt
```

## 纯 C 驱动集成（单片机 / 嵌入式）

纯 C 驱动只做两件事：把要发给设备的消息**组装成数据帧**，以及把从设备收到的字节流**解析成消息**。数据的实际收发由用户自行实现。

### 加入工程

把以下文件加入你的编译

- 源文件：`src/fm_crc.c`、`src/fm_frame.c`、`src/fm_driver.c`
- 头文件目录：`include/`

业务代码中只需 `#include "fm_driver.h"`。驱动遵循 C11，无动态内存分配、无第三方依赖，适合资源受限的 MCU。

> 纯 C 工程只需集成 `include/` 和 `src/`，不需要依赖 `app/`。如果需要了解如何把驱动接口接入实际的数据收发流程，可参考应用示例：解析设备数据参考 [`app/reader/`](app/reader/)，向设备发送数据参考 [`app/writer/`](app/writer/)。示例应用使用 C++ 实现串口和日志，但其中调用的组包、解析接口与纯 C 工程完全相同。

### 接口概览

所有接口与消息结构体都定义在 [include/fm_driver.h](include/fm_driver.h) 中。通信分为“设备 → 用户”和“用户 → 设备”两个方向：

- **组包**：`fm_prepare_msg_to_dev()` 将一个消息（`FMData*` 结构体）封装为一帧，返回帧长度，随后直接将该帧通过串口发给设备。
  - 如需在一帧里打包多条消息，使用分步接口：`fm_prepare_msg_to_dev_begin()` → `fm_prepare_msg_to_dev_try_append()`（可多次）→ `fm_prepare_msg_to_dev_end()`。
- **解析**：用 `FMParserFromDev` + `fm_parser_from_dev_init()` 注册回调，串口每收到一段数据就喂给 `fm_parser_from_dev_handle_data()`；内部自动完成拆帧与解析。每帧开始时回调 `on_frame_begin`（提供角色/uid/帧计数等帧头信息），帧内每解析出一条消息回调一次 `on_frame_msg`（回调里按 `msg_id` 取对应的 `FMData*` 结构体），处理完一帧后回调 `on_frame_end`；不需要的回调可传 `NULL`。

> 消息 ID（`FM_MSG_*`）与对应结构体（`FMData*`）的一一映射、各字段含义，均见头文件注释。

### 最小示例

```c
#include "fm_driver.h"
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

测试文件 [test/test_fm_driver.cpp](test/test_fm_driver.cpp) 覆盖了**所有消息**在两个方向上的组包与解析往返用例，是最完整、最权威的接口用法参考，集成时可直接对照。

## ROS1 集成

ROS 驱动包在纯 C 驱动之上封装了串口收发与话题转换，节点名为 `ros_converter`，可执行文件名为 `ros_converter`。

> 前置：已安装 ROS1（如 Noetic）并完成 `source /opt/ros/<distro>/setup.bash`。

### clone 到工作空间

```bash
mkdir -p ~/catkin_ws/src
cd ~/catkin_ws/src
git clone <本仓库地址> fm_driver
```

### 编译

```bash
cd ~/catkin_ws
catkin_make        # 或 catkin build
source devel/setup.bash
```

> 包内已根据环境变量 `ROS_VERSION` 自动选择构建 ROS1 还是 ROS2，无需手动指定。

### 运行

把 AOA 有线设备通过串口接入电脑，确认串口号（如 `/dev/ttyUSB0`）后启动：

```bash
roslaunch fm_driver msg.launch port:=/dev/ttyUSB0 baudrate:=921600
```

- `port`：串口设备号，默认 `/dev/ttyUSB0`
- `baudrate`：波特率，默认 `921600`
- `frame_id`：定位结果所在坐标系，填入 `header.frame_id`，默认 `fm_anchor_link`

> 若启动时报串口「Permission denied」，见下文 [串口权限（Linux）](#串口权限linux)。

### 可视化（rviz）

用 `rviz.launch` 一步启动「驱动 + 静态 TF + rviz」，开箱即可看到定位结果：

```bash
roslaunch fm_driver rviz.launch port:=/dev/ttyUSB0
```

除上面的参数外，还支持：

- `parent_frame`：基站坐标系的父坐标系，默认 `map`
- `anchor_pose`：基站在 `parent_frame` 中的安装位姿 `x y z yaw pitch roll`，默认 `0 0 0 0 0 0`
- `static_tf`：是否发布 `parent_frame` → `frame_id` 的静态 TF，默认 `true`。
  若该 TF 已由其它节点（如机器人 URDF）发布，置 `false` 避免冲突

### 查看数据

```bash
rostopic list                 # 列出所有话题
rostopic echo /ros_converter/result # 查看定位结果
```

## ROS2 集成

> 前置：已安装 ROS2（如 Humble）并完成 `source /opt/ros/<distro>/setup.bash`。

### clone 到工作空间

```bash
mkdir -p ~/ros2_ws/src
cd ~/ros2_ws/src
git clone <本仓库地址> fm_driver
```

### 编译

```bash
cd ~/ros2_ws
colcon build
source install/setup.bash
```

### 运行

```bash
ros2 launch fm_driver msg.py port:=/dev/ttyACM0 baudrate:=921600
```

- `port`：串口设备号，默认 `/dev/ttyACM0`
- `baudrate`：波特率，默认 `921600`
- `frame_id`：定位结果所在坐标系，填入 `header.frame_id`，默认 `fm_anchor_link`

> 若启动时报串口「Permission denied」，见下文 [串口权限（Linux）](#串口权限linux)。

也可不用 launch 直接运行节点：

```bash
ros2 run fm_driver ros_converter --ros-args -p port:=/dev/ttyACM0 -p baudrate:=921600
```

### 可视化（rviz2）

用 `rviz.py` 一步启动「驱动 + 静态 TF + rviz2」，开箱即可看到定位结果：

```bash
ros2 launch fm_driver rviz.py port:=/dev/ttyACM0
```

除上面的参数外，还支持：

- `parent_frame`：基站坐标系的父坐标系，默认 `map`
- `x` / `y` / `z` / `yaw` / `pitch` / `roll`：基站在 `parent_frame` 中的安装位姿，默认全 0
- `static_tf`：是否发布 `parent_frame` → `frame_id` 的静态 TF，默认 `true`。
  若该 TF 已由其它节点（如机器人 URDF）发布，置 `false` 避免冲突

### 查看数据

```bash
ros2 topic list
ros2 topic echo /ros_converter/result
```

## 串口权限（Linux）

在 Linux 下首次访问串口（如 `/dev/ttyUSB0`、`/dev/ttyACM0`）时，常会遇到类似报错：

```text
could not open port /dev/ttyUSB0: [Errno 13] Permission denied: '/dev/ttyUSB0'
```

**原因**：串口设备通常属于 `dialout` 组（部分发行版为 `uucp`），而普通用户默认不在该组，因此没有读写权限。

参考：[Fix Serial Port “Permission Denied” Errors on Linux](https://websistent.com/fix-serial-port-permission-denied-errors-linux/)

### 1. 确认设备所属用户组

```bash
ls -l /dev/ttyUSB0
# crw-rw---- 1 root dialout 188, 0 ... /dev/ttyUSB0
#                   ^^^^^^^ 设备所属组(此处为 dialout)
```

### 2. 将当前用户加入该组（推荐，永久生效）

```bash
sudo usermod -a -G dialout $USER
```

> `-a` 不可省略，否则会把用户从其它附加组中移除。若上一步显示的组是 `uucp`，则把命令中的 `dialout` 换成 `uucp`。

### 3. 重新登录使其生效

组变更需注销后重新登录（或重启）才会生效。生效后可用以下命令确认已加入：

```bash
groups | grep dialout
```

## 话题说明（ROS1 / ROS2 通用）

节点名为 `ros_converter`，话题均以节点名为命名空间（即 `/ros_converter/<话题名>`）。

### 设备 → 用户（订阅以获取数据）

| 话题 | 消息类型 | 说明 |
| --- | --- | --- |
| `~/result` | `Result` | 定位结果（位置/速度/噪声） |
| `~/result_pose` | `geometry_msgs/PoseWithCovarianceStamped` | 同一份定位结果的标准消息，供 rviz 直接显示 |
| `~/prev_result` | `PrevResult` | 上一次定位结果（TAG） |
| `~/spherical_result` | `SphericalResult` | 球坐标定位结果（距离/方位角/俯仰角） |
| `~/prev_spherical_result` | `PrevSphericalResult` | 上一次球坐标结果（TAG） |
| `~/dis` | `Dis` | 测距结果 |
| ~/heartbeat | Heartbeat | 设备心跳/状态信息 |
| `~/param` | `Param` | 参数读取响应/当前参数 |
| `~/echo_from_device` | `Echo` | 设备回显 |
| `~/user_data_from_device` | `DataUserToUser` | 设备转发的用户自定义数据 |

### 用户 → 设备（发布以下发命令）

| 话题 | 消息类型 | 说明 |
| --- | --- | --- |
| `~/find` | `Find` | 让设备震动/闪灯以便寻找 |
| ~/restart | Restart | 重启设备 |
| `~/param_read` | `std_msgs/Empty` | 读取设备参数 |
| `~/param_write` | `Param` | 写入设备参数 |
| `~/begin_pair` | `BeginPair` | 进入配对模式 |
| `~/cancel_pair` | `CancelPair` | 取消配对 |
| `~/echo_to_device` | `Echo` | 回显测试 |
| `~/user_data_to_device` | `DataUserToUser` | 下发用户自定义数据 |

消息字段定义见 [msg/](msg/) 目录。

## Foxglove 可视化（reader，无需 ROS）

`app/reader/` 在原有日志打印之外，可选地内置一个 Foxglove WebSocket 服务，
不依赖 ROS，适合在没有 ROS 环境的机器上直接看数据。

开启方式（默认关闭）：

```bash
cmake -B build -DFM_BUILD_FOXGLOVE=ON
cmake --build build
./build/app/reader/reader /dev/ttyUSB0 921600   # 可选第三个参数指定端口，默认 8765
```

然后在 Foxglove 中选择 **Open connection → Foxglove WebSocket**，
地址填 `ws://<设备IP>:8765` 即可。日志打印保持原样，两者同时输出。

发布的话题：

| 话题 | 类型 | 用途 |
| --- | --- | --- |
| `/result_pose` | `foxglove.PoseInFrame` | 定位点，3D 面板显示 |
| `/result_scene` | `foxglove.SceneUpdate` | 不确定性球（直径 2σ）+ 速度矢量线段 |
| `/tf` | `foxglove.FrameTransform` | `world` → `fm_anchor_link`，3D 面板的坐标系 |
| `/result` | JSON | 位置/速度/噪声，供 Plot 面板绘制曲线 |
| `/spherical_result` | JSON | 距离/方位角/俯仰角，供 Plot 面板绘制曲线 |
| `/dis` | JSON | 测距结果，供 Plot 面板绘制曲线 |

> 开启后构建会自动下载 Foxglove SDK 官方预编译包（约 30MB，含 Linux x64/arm64、
> Windows、macOS），无需 Rust 工具链。未开启时 reader 不链接任何 Foxglove 代码，
> 行为与集成前完全一致。

## 构建选项（CMake）

| 选项 | 默认 | 说明 |
| --- | --- | --- |
| `FM_BUILD_READER` | ON | 构建串口接收与设备数据解析示例 |
| `FM_BUILD_WRITER` | ON | 构建组包与串口发送示例 |
| `FM_BUILD_FOXGLOVE` | ON | 为 reader 启用 Foxglove 可视化（自动拉取 Foxglove SDK 预编译包） |
| `FM_BUILD_TEST` | ON | 构建纯 C 驱动单元测试（自动拉取 Catch2） |
| `FM_BUILD_ROS1` | OFF | 构建 ROS1 驱动包 |
| `FM_BUILD_ROS2` | OFF | 构建 ROS2 驱动包 |

未显式指定 ROS 选项时，会依据环境变量 `ROS_VERSION` 自动选择，因此在 ROS 工作空间中用 `catkin_make` / `colcon build` 即可，无需手动设置。
