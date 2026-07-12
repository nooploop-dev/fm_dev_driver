# AOA DEVICE DRIVER

[简体中文](./readme.md) | English

This repository contains the driver code for the Nooploop [AOA Follow Me](https://www.nooploop.com/en/follow-me/) product. It provides:

- **Pure C driver** (`include/` + `src/`): no third-party dependencies, ready to be integrated into MCU/embedded projects. It handles the "encoding" and "parsing" of the device communication protocol.
- **ROS1 / ROS2 driver packages** (`app/` + `msg/` + `launch/`): a layer on top of the pure C driver that wraps serial I/O and topic conversion. Just clone it into a ROS workspace to use it.

## Directory Structure

```text
fm_driver/
├── include/                  # Public headers
│   └── fm_driver.h           #   Driver API (usually the only one you need)
├── src/                      # Pure C driver implementation (compile these too)
│   ├── fm_crc.c/.h           #   CRC checksum
│   ├── fm_frame.c/.h         #   Frame assembly/disassembly
│   ├── fm_driver.c           #   Encoding/parsing main logic
│   ├── fm_driver_raw.h       #   Protocol-layer structs and conversions (internal)
│   └── fm_msg.h              #   Message layer (internal)
├── test/                     # Complete API usage examples (highly recommended)
├── app/                      # Host applications and integration examples
│   ├── reader/               #   Receive and parse over serial (optional Foxglove viz)
│   ├── writer/               #   Encode and send to the device over serial
│   ├── ros1_converter/       #   ROS1 topic conversion node
│   └── ros2_converter/       #   ROS2 topic conversion node
├── msg/                      # ROS message definitions
├── launch/                   # ROS launch files
├── rviz/                     # rviz display configs (one each for ROS1/ROS2)
├── package.xml               # ROS package manifest
└── CMakeLists.txt
```

## Pure C Driver Integration (MCU / Embedded)

The pure C driver does just two things: **encode** the messages you want to send to the device into data frames, and **parse** the byte stream received from the device into messages. The actual I/O is left to you.

### Add to Your Project

- Source files: `src/fm_crc.c`, `src/fm_frame.c`, `src/fm_driver.c`
- Header directory: `include/`

In your application code you only need `#include "fm_driver.h"`. The driver targets C11, performs no dynamic memory allocation, and has no third-party dependencies, making it suitable for resource-constrained MCUs.

> A pure C project does not need `app/`. For examples of wiring the driver APIs into real I/O, see [`app/reader/`](app/reader/) (parsing) and [`app/writer/`](app/writer/) (encoding). The examples use C++ for serial I/O and logging, but call exactly the same driver APIs as a pure C project.

### API Overview

All APIs and message structs are defined in [include/fm_driver.h](include/fm_driver.h). Communication has two directions, "device → user" and "user → device":

- **Encoding**: `fm_prepare_msg_to_dev()` packs one message (a `FMData*` struct) into a frame and returns the frame length. You then send the frame to the device over the serial port.
  - To pack multiple messages into a single frame, use the step-by-step API: `fm_prepare_msg_to_dev_begin()` → `fm_prepare_msg_to_dev_try_append()` (callable multiple times) → `fm_prepare_msg_to_dev_end()`.
- **Parsing**: register callbacks with `FMParserFromDev` + `fm_parser_from_dev_init()`. Feed every chunk of received serial data to `fm_parser_from_dev_handle_data()`; it automatically deframes and parses. `on_frame_begin` is invoked once at the start of each frame (providing the frame header info: role/uid/frame count), `on_frame_msg` once per parsed message inside the frame (cast to the matching `FMData*` struct based on `msg_id`), and `on_frame_end` once after the whole frame is processed; pass `NULL` for callbacks you don't need.

> The mapping between message IDs (`FM_MSG_*`) and their structs (`FMData*`), plus the meaning and unit of each field, is documented in the header comments.
>
> If you are developing **device-side firmware** (encoding messages to send to the user), see [include/fm_driver_for_dev.h](include/fm_driver_for_dev.h).

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

[test/test_fm_driver.cpp](test/test_fm_driver.cpp) covers encode/parse round-trip cases for **every message** in both directions. It is the most complete and authoritative API usage reference — refer to it directly during integration.

## ROS Integration

ROS1 and ROS2 share the same code. The node name and executable name are both `ros_converter`. The build picks the ROS version automatically from the `ROS_VERSION` environment variable, so no manual configuration is needed.

> Prerequisite: ROS installed and `source /opt/ros/<distro>/setup.bash` done.

### Build

Clone this repository into your workspace's `src/` directory (as `fm_driver`), then:

| | ROS1 | ROS2 |
| --- | --- | --- |
| Build | `catkin_make` | `colcon build` |
| Environment | `source devel/setup.bash` | `source install/setup.bash` |

### Run

Connect the wired AOA device over serial, confirm the port, then launch:

```bash
roslaunch fm_driver msg.launch port:=/dev/ttyUSB0     # ROS1
ros2 launch fm_driver msg.py   port:=/dev/ttyACM0     # ROS2
```

Arguments (common to both):

- `port`: serial device path
- `baudrate`: baud rate, default `921600`
- `frame_id`: frame the positioning result is expressed in, written to `header.frame_id`, default `fm_anchor_link`

On ROS2 you can also run the node directly without launch:

```bash
ros2 run fm_driver ros_converter --ros-args -p port:=/dev/ttyACM0
```

> If you get a serial "Permission denied" error at startup, see [Serial Port Permissions (Linux)](#serial-port-permissions-linux) below.

### Visualization (rviz)

The `rviz` launch files start the driver, a static TF and rviz together, so the
positioning result (position plus uncertainty ellipsoid) is visible out of the box:

```bash
roslaunch fm_driver rviz.launch port:=/dev/ttyUSB0    # ROS1
ros2 launch fm_driver rviz.py   port:=/dev/ttyACM0    # ROS2
```

In addition to the arguments above:

- `parent_frame`: parent frame of the anchor frame, default `map`
- Anchor pose within `parent_frame`: on ROS1 use `anchor_pose:="x y z yaw pitch roll"` (all `0` by default); on ROS2 set `x`/`y`/`z`/`yaw`/`pitch`/`roll` individually
- `static_tf`: whether to publish the `parent_frame` → `frame_id` static TF, default `true`. Set to `false` if that TF is already published elsewhere (e.g. by your robot URDF) to avoid conflicts

> The positioning result is published in the anchor's own frame `fm_anchor_link`. Where the anchor is mounted belongs to your URDF/calibration, so the driver itself publishes no TF. The static TF above merely gives rviz a usable TF tree for standalone testing.

### View Data

```bash
rostopic echo /ros_converter/result       # ROS1
ros2 topic echo /ros_converter/result     # ROS2
```

### Topics

All topics are namespaced under the node name, i.e. `/ros_converter/<topic>`. Message field definitions are in the [msg/](msg/) directory.

**Device → User** (subscribe to receive data)

| Topic | Message Type | Description |
| --- | --- | --- |
| `~/result` | `Result` | Positioning result (position/velocity/noise) |
| `~/result_pose` | `geometry_msgs/PoseWithCovarianceStamped` | The same result as a standard message, for direct display in rviz |
| `~/prev_result` | `PrevResult` | Previous positioning result (TAG) |
| `~/spherical_result` | `SphericalResult` | Spherical positioning result (distance/azimuth/elevation) |
| `~/prev_spherical_result` | `PrevSphericalResult` | Previous spherical result (TAG) |
| `~/dis` | `Dis` | Ranging result |
| `~/heartbeat` | `Heartbeat` | Device heartbeat/status info |
| `~/param` | `Param` | Parameter read response/current parameters |
| `~/echo_from_device` | `Echo` | Device echo |
| `~/user_data_from_device` | `DataUserToUser` | User-defined data forwarded by the device |

**User → Device** (publish to send commands)

| Topic | Message Type | Description |
| --- | --- | --- |
| `~/find` | `Find` | Make the device vibrate/blink to locate it |
| `~/restart` | `Restart` | Restart the device |
| `~/param_read` | `std_msgs/Empty` | Read device parameters |
| `~/param_write` | `Param` | Write device parameters |
| `~/begin_pair` | `BeginPair` | Enter pairing mode |
| `~/cancel_pair` | `CancelPair` | Cancel pairing |
| `~/echo_to_device` | `Echo` | Echo test |
| `~/user_data_to_device` | `DataUserToUser` | Send user-defined data |

## Foxglove Visualization (no ROS required)

On top of its existing log output, `app/reader/` can host a Foxglove WebSocket
server. It does not depend on ROS, which makes it handy for inspecting data on
machines without a ROS installation. **It is on by default in non-ROS builds**:

```bash
cmake -B build && cmake --build build
./build/app/reader/reader /dev/ttyUSB0 921600   # optional 3rd arg sets the port, default 8765
```

Then in Foxglove choose **Open connection → Foxglove WebSocket** and enter
`ws://<device-ip>:8765`. Log printing is unchanged; both run side by side.

| Topic | Type | Purpose |
| --- | --- | --- |
| `/result_pose` | `foxglove.PoseInFrame` | Position, shown in the 3D panel |
| `/result_scene` | `foxglove.SceneUpdate` | Uncertainty sphere (2σ diameter) + velocity vector |
| `/tf` | `foxglove.FrameTransform` | `world` → `fm_anchor_link`, the 3D panel's frame |
| `/result` | JSON | Position/velocity/noise, for the Plot panel |
| `/spherical_result` | JSON | Distance/azimuth/elevation, for the Plot panel |
| `/dis` | JSON | Ranging result, for the Plot panel |

## Serial Port Permissions (Linux)

Accessing a serial port for the first time often fails like this:

```text
could not open port /dev/ttyUSB0: [Errno 13] Permission denied: '/dev/ttyUSB0'
```

Serial devices belong to the `dialout` group (`uucp` on some distros) and regular
users are not in it by default. Confirm the group with `ls -l /dev/ttyUSB0`, add
yourself to it, then **log out and back in** for it to take effect:

```bash
sudo usermod -a -G dialout $USER   # -a is required, or you drop out of your other groups
```

Reference: [Fix Serial Port “Permission Denied” Errors on Linux](https://websistent.com/fix-serial-port-permission-denied-errors-linux/)

## Build Options (CMake)

| Option | Default | Description |
| --- | --- | --- |
| `FM_BUILD_READER` | ON | Build the serial receive/parse example |
| `FM_BUILD_WRITER` | ON | Build the encode/serial send example |
| `FM_BUILD_FOXGLOVE` | ON for non-ROS / OFF for ROS builds | Enable Foxglove visualization in the reader (fetches the prebuilt SDK); can be overridden explicitly |
| `FM_BUILD_TEST` | ON | Build the pure C driver unit tests (fetches Catch2 automatically) |
| `FM_BUILD_ROS1` | OFF | Build the ROS1 driver package |
| `FM_BUILD_ROS2` | OFF | Build the ROS2 driver package |

If neither ROS option is set explicitly, the build picks one based on the `ROS_VERSION` environment variable, so `catkin_make` / `colcon build` in a ROS workspace just works.
