#pragma once

#include "fm_driver.h"
#include <cstdint>
#include <string>

// 把解析出的数据推送到Foxglove WebSocket服务，与spdlog日志打印并行，互不影响。
// 实现有两份，由CMake的FM_BUILD_FOXGLOVE在链接期二选一(故调用方无需条件编译):
//   ON -> foxglove_viz.cpp(真实现)  OFF -> foxglove_viz_stub.cpp(空实现)
namespace foxglove_viz {

// 启动WebSocket服务，Foxglove中通过ws://<host>:<port>连接。
// 返回false表示未启动(未启用Foxglove，或启动失败)
bool init(const std::string &host, uint16_t port, const std::string &frame_id);
void publish(const FMDataResult &data);
void publish(const FMDataSphericalResult &data);
void publish(const FMDataDis &data);
void shutdown();

} // namespace foxglove_viz
