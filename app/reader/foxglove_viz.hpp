#pragma once

#include "fm_driver.h"
#include <cstdint>
#include <string>

// Foxglove 可视化：把解析出的数据同时推送到 Foxglove WebSocket 服务，
// 与原有的 spdlog 日志打印并行，互不影响。
//
// 实现有两份，由 CMake 的 FM_BUILD_FOXGLOVE 在链接期二选一：
//   ON  -> foxglove_viz.cpp        真实实现，链接 Foxglove SDK
//   OFF -> foxglove_viz_stub.cpp   空实现，不引入任何 Foxglove 代码
// 因此调用方无需任何条件编译。
namespace foxglove_viz {

// 启动 WebSocket 服务。Foxglove 中通过 ws://<host>:<port> 连接。
// frame_id 为定位结果所在坐标系，与 ROS 侧保持一致。
// 返回 false 表示未启动(未启用 Foxglove，或启动失败)。
bool init(const std::string &host, uint16_t port, const std::string &frame_id);
void publish(const FMDataResult &data);
void publish(const FMDataSphericalResult &data);
void publish(const FMDataDis &data);
void shutdown();

} // namespace foxglove_viz
