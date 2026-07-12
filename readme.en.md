# AOA DEVICE DRIVER

[简体中文](./readme.md) | English

This repository contains the driver code for the Nooploop [AOA Follow Me](https://www.nooploop.com/en/follow-me/) product. It provides:

- **Pure C driver** (`include/` + `src/`): no third-party dependencies, ready to be integrated into MCU/embedded projects. It handles the "encoding" and "parsing" of the device communication protocol.
- **ROS1 / ROS2 driver packages** (`app/` + `msg/` + `launch/`): a layer on top of the pure C driver that wraps serial I/O and topic conversion. Just clone it into a ROS workspace to use it.

## Directory Structure

```text
fm_driver/
├── include/fm_driver.h   # The only public header (the only file you need to care about)
├── src/                      # Pure C driver implementation (must be compiled together)
│   ├── fm_crc.c/.h       #   CRC checksum
│   ├── fm_frame.c/.h     #   Frame assembly/disassembly
│   ├── fm_driver.c       #   Encoding/parsing main logic
│   ├── fm_driver_raw.h   #   Protocol-layer structs and conversions (internal)
│   └── fm_msg.h          #   Message layer (internal)
├── test/test_fm_driver.cpp  # Complete usage examples for the API (highly recommended)
├── app/                      # Host applications and integration examples
│   ├── reader/               # Example: receive and parse device data over serial
│   ├── writer/               # Example: encode and send data to the device over serial
│   ├── ros1_converter/       # ROS1 topic/device-data conversion node
│   └── ros2_converter/       # ROS2 topic/device-data conversion node
├── msg/                      # ROS message definitions
├── launch/                   # ROS launch files
├── rviz/                     # rviz display configs (one each for ROS1/ROS2)
├── package.xml               # ROS package manifest
└── CMakeLists.txt
```

## Pure C Driver Integration (MCU / Embedded)

The pure C driver does just two things: **encode** the messages you want to send to the device into data frames, and **parse** the byte stream received from the device into messages. The actual I/O is left to you.

### Add to Your Project

Add the following to your build:

- Source files: `src/fm_crc.c`, `src/fm_frame.c`, `src/fm_driver.c`
- Header directory: `include/`

In your application code you only need `#include "fm_driver.h"`. The driver targets C11, performs no dynamic memory allocation, and has no third-party dependencies, making it suitable for resource-constrained MCUs.

> A pure C project only needs to integrate `include/` and `src/`; it does not depend on `app/`. For examples of connecting the driver APIs to actual data I/O, see [`app/reader/`](app/reader/) for parsing device data and [`app/writer/`](app/writer/) for sending data to the device. The example applications use C++ for serial I/O and logging, but call exactly the same encoding and parsing APIs as a pure C project.

### API Overview

All APIs and message structs are defined in [include/fm_driver.h](include/fm_driver.h). Communication has two directions: "device → user" and "user → device":

- **Encoding**: `fm_prepare_msg_to_dev()` packs one message (a `FMData*` struct) into a frame and returns the frame length. You then send the frame to the device over the serial port.
  - To pack multiple messages into a single frame, use the step-by-step API: `fm_prepare_msg_to_dev_begin()` → `fm_prepare_msg_to_dev_try_append()` (callable multiple times) → `fm_prepare_msg_to_dev_end()`.
- **Parsing**: register callbacks with `FMParserFromDev` + `fm_parser_from_dev_init()`. Feed every chunk of received serial data to `fm_parser_from_dev_handle_data()`; it automatically deframes and parses. `on_frame_begin` is invoked once at the start of each frame (providing the frame header info: role/uid/frame count), `on_frame_msg` once per parsed message inside the frame (cast to the matching `FMData*` struct based on `msg_id`), and `on_frame_end` once after the whole frame is processed; pass `NULL` for callbacks you don't need.

> The one-to-one mapping between message IDs (`FM_MSG_*`) and their structs (`FMData*`), as well as the meaning of each field, is documented in the header comments.

### Minimal Example

```c
#include "fm_driver.h"
#include <stdio.h>

// —— Receiving: invoked once per parsed message reported by the device ——
// FM_WIRED: the message comes from the wired (directly connected) device;
// FM_WIRELESS: it comes from the device on the wireless side
static void on_frame_msg_from_dev(fm_connect_type_e connect_type,
                                  fm_msg_id_t msg_id, const void *msg_payload,
                                  int msg_payload_size) {
  switch (msg_id) {
  case FM_MSG_SPHERICAL_RESULT: { // spherical positioning result
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
  // Register on_frame_begin/on_frame_end if you need the frame header info
  // (role/uid/frame count)
  fm_parser_from_dev_init(&g_parser, NULL, on_frame_msg_from_dev, NULL);
}

// Call this whenever the serial ISR/poll receives data
void app_on_serial_rx(const uint8_t *data, int size) {
  fm_parser_from_dev_handle_data(&g_parser, data, size);
}

// —— Sending: issue a command to the device (e.g. make it vibrate/blink for 5s) ——
void app_send_find(void) {
  FMDataFind find = {0};
  find.duration = 5;

  uint8_t frame[FM_FRAME_SIZE_MAX];
  static fm_frame_cnt_t cnt = 0; // frame counter, +1 per frame
  int n = fm_prepare_msg_to_dev(FM_WIRED, cnt++, FM_MSG_FIND, &find, frame,
                                sizeof(frame));
  // user_serial_write is a serial send function you implement yourself
  user_serial_write(frame, n);
}
```

### More Usage

The test file [test/test_fm_driver.cpp](test/test_fm_driver.cpp) covers encode/parse round-trip cases for **every message** in both directions. It is the most complete and authoritative API usage reference — refer to it directly during integration.

## ROS1 Integration

The ROS driver package wraps serial I/O and topic conversion on top of the pure C driver. The node name is `ros_converter` and the executable name is `ros_converter`.

> Prerequisite: ROS1 (e.g. Noetic) installed and `source /opt/ros/<distro>/setup.bash` done.

### Clone into the Workspace

```bash
mkdir -p ~/catkin_ws/src
cd ~/catkin_ws/src
git clone <repository-url> fm_driver
```

### Build

```bash
cd ~/catkin_ws
catkin_make        # or catkin build
source devel/setup.bash
```

> The package automatically chooses to build for ROS1 or ROS2 based on the `ROS_VERSION` environment variable; no manual configuration needed.

### Run

Connect the wired AOA device to your computer via serial, confirm the port (e.g. `/dev/ttyUSB0`), then launch:

```bash
roslaunch fm_driver msg.launch port:=/dev/ttyUSB0 baudrate:=921600
```

- `port`: serial device path, default `/dev/ttyUSB0`
- `baudrate`: baud rate, default `921600`
- `frame_id`: frame the positioning result is expressed in, written to `header.frame_id`, default `fm_anchor_link`

> If you get a serial "Permission denied" error at startup, see [Serial Port Permissions (Linux)](#serial-port-permissions-linux) below.

### Visualization (rviz)

`rviz.launch` starts the driver, a static TF and rviz together, so the positioning
result is visible out of the box:

```bash
roslaunch fm_driver rviz.launch port:=/dev/ttyUSB0
```

In addition to the arguments above it accepts:

- `parent_frame`: parent frame of the anchor frame, default `map`
- `anchor_pose`: anchor pose within `parent_frame` as `x y z yaw pitch roll`, default `0 0 0 0 0 0`
- `static_tf`: whether to publish the `parent_frame` → `frame_id` static TF, default `true`.
  Set to `false` if that TF is already published elsewhere (e.g. by your robot URDF) to avoid conflicts

### View Data

```bash
rostopic list                       # list all topics
rostopic echo /ros_converter/result # view positioning results
```

## ROS2 Integration

> Prerequisite: ROS2 (e.g. Humble) installed and `source /opt/ros/<distro>/setup.bash` done.

### Clone into the Workspace

```bash
mkdir -p ~/ros2_ws/src
cd ~/ros2_ws/src
git clone <repository-url> fm_driver
```

### Build

```bash
cd ~/ros2_ws
colcon build
source install/setup.bash
```

### Run

```bash
ros2 launch fm_driver msg.py port:=/dev/ttyACM0 baudrate:=921600
```

- `port`: serial device path, default `/dev/ttyACM0`
- `baudrate`: baud rate, default `921600`
- `frame_id`: frame the positioning result is expressed in, written to `header.frame_id`, default `fm_anchor_link`

> If you get a serial "Permission denied" error at startup, see [Serial Port Permissions (Linux)](#serial-port-permissions-linux) below.

You can also run the node directly without launch:

```bash
ros2 run fm_driver ros_converter --ros-args -p port:=/dev/ttyACM0 -p baudrate:=921600
```

### Visualization (rviz2)

`rviz.py` starts the driver, a static TF and rviz2 together, so the positioning
result is visible out of the box:

```bash
ros2 launch fm_driver rviz.py port:=/dev/ttyACM0
```

In addition to the arguments above it accepts:

- `parent_frame`: parent frame of the anchor frame, default `map`
- `x` / `y` / `z` / `yaw` / `pitch` / `roll`: anchor pose within `parent_frame`, all default `0`
- `static_tf`: whether to publish the `parent_frame` → `frame_id` static TF, default `true`.
  Set to `false` if that TF is already published elsewhere (e.g. by your robot URDF) to avoid conflicts

### View Data

```bash
ros2 topic list
ros2 topic echo /ros_converter/result
```

## Serial Port Permissions (Linux)

When accessing a serial port (e.g. `/dev/ttyUSB0`, `/dev/ttyACM0`) for the first time on Linux, you often hit an error like:

```text
could not open port /dev/ttyUSB0: [Errno 13] Permission denied: '/dev/ttyUSB0'
```

**Cause**: serial devices usually belong to the `dialout` group (`uucp` on some distributions), and a regular user is not in that group by default, so it lacks read/write permission.

Reference: [Fix Serial Port "Permission Denied" Errors on Linux](https://websistent.com/fix-serial-port-permission-denied-errors-linux/)

### 1. Check the Device's Group

```bash
ls -l /dev/ttyUSB0
# crw-rw---- 1 root dialout 188, 0 ... /dev/ttyUSB0
#                   ^^^^^^^ owning group (dialout here)
```

### 2. Add the Current User to That Group (recommended, permanent)

```bash
sudo usermod -a -G dialout $USER
```

> Do not omit `-a`, otherwise the user will be removed from its other supplementary groups. If the group shown in the previous step is `uucp`, replace `dialout` in the command with `uucp`.

### 3. Re-login to Apply

Group changes only take effect after logging out and back in (or rebooting). Once applied, verify membership with:

```bash
groups | grep dialout
```

## Topics (common to ROS1 / ROS2)

The node name is `ros_converter`, and all topics are namespaced under the node name (i.e. `/ros_converter/<topic>`).

### Device → User (subscribe to receive data)

| Topic | Message Type | Description |
| --- | --- | --- |
| `~/result` | `Result` | Positioning result (position/velocity/noise) |
| `~/result_pose` | `geometry_msgs/PoseWithCovarianceStamped` | The same positioning result as a standard message, for direct display in rviz |
| `~/prev_result` | `PrevResult` | Previous positioning result (TAG) |
| `~/spherical_result` | `SphericalResult` | Spherical positioning result (distance/azimuth/elevation) |
| `~/prev_spherical_result` | `PrevSphericalResult` | Previous spherical result (TAG) |
| `~/dis` | `Dis` | Ranging result |
| ~/heartbeat | Heartbeat | Device heartbeat/status info |
| `~/param` | `Param` | Parameter read response/current parameters |
| `~/echo_from_device` | `Echo` | Device echo |
| `~/user_data_from_device` | `DataUserToUser` | User-defined data forwarded by the device |

### User → Device (publish to send commands)

| Topic | Message Type | Description |
| --- | --- | --- |
| `~/find` | `Find` | Make the device vibrate/blink to locate it |
| ~/restart | Restart | Restart the device |
| `~/param_read` | `std_msgs/Empty` | Read device parameters |
| `~/param_write` | `Param` | Write device parameters |
| `~/begin_pair` | `BeginPair` | Enter pairing mode |
| `~/cancel_pair` | `CancelPair` | Cancel pairing |
| `~/echo_to_device` | `Echo` | Echo test |
| `~/user_data_to_device` | `DataUserToUser` | Send user-defined data |

Message field definitions are in the [msg/](msg/) directory.

## Foxglove Visualization (reader, no ROS required)

On top of its existing log output, `app/reader/` can optionally host a Foxglove
WebSocket server. It does not depend on ROS, which makes it handy for inspecting
data on machines without a ROS installation.

Enable it (off by default):

```bash
cmake -B build -DFM_BUILD_FOXGLOVE=ON
cmake --build build
./build/app/reader/reader /dev/ttyUSB0 921600   # optional 3rd arg sets the port, default 8765
```

Then in Foxglove choose **Open connection → Foxglove WebSocket** and enter
`ws://<device-ip>:8765`. Log printing is unchanged; both run side by side.

Published topics:

| Topic | Type | Purpose |
| --- | --- | --- |
| `/result_pose` | `foxglove.PoseInFrame` | Position, shown in the 3D panel |
| `/result_scene` | `foxglove.SceneUpdate` | Uncertainty sphere (2σ diameter) + velocity vector |
| `/tf` | `foxglove.FrameTransform` | `world` → `fm_anchor_link`, the 3D panel's frame |
| `/result` | JSON | Position/velocity/noise, for the Plot panel |
| `/spherical_result` | JSON | Distance/azimuth/elevation, for the Plot panel |
| `/dis` | JSON | Ranging result, for the Plot panel |

> When enabled, the build downloads the official prebuilt Foxglove SDK (~30 MB;
> Linux x64/arm64, Windows and macOS are all covered), so no Rust toolchain is
> needed. When disabled, the reader links no Foxglove code at all and behaves
> exactly as before.

## Build Options (CMake)

| Option | Default | Description |
| --- | --- | --- |
| `FM_BUILD_READER` | ON | Build the serial receive and device-data parsing example |
| `FM_BUILD_WRITER` | ON | Build the encoding and serial send example |
| `FM_BUILD_FOXGLOVE` | ON | Enable Foxglove visualization in the reader (fetches the prebuilt Foxglove SDK automatically) |
| `FM_BUILD_TEST` | ON | Build the pure C driver unit tests (fetches Catch2 automatically) |
| `FM_BUILD_ROS1` | OFF | Build the ROS1 driver package |
| `FM_BUILD_ROS2` | OFF | Build the ROS2 driver package |

When no ROS option is explicitly set, the choice is made automatically based on the `ROS_VERSION` environment variable, so `catkin_make` / `colcon build` in a ROS workspace just works without manual configuration.
