// FM_BUILD_FOXGLOVE 关闭时链接本文件，reader 的行为与集成 Foxglove 前完全一致。
// MCAP录制依赖Foxglove SDK，故本文件中同样是空实现。
#include "foxglove_viz.hpp"

#include <spdlog/spdlog.h>

namespace foxglove_viz {

bool init(const std::string &, uint16_t, const std::string &) { return false; }

bool record_start(const std::string &) {
  spdlog::warn("Record: unavailable, rebuild with -DFM_BUILD_FOXGLOVE=ON");
  return false;
}

void publish(const FMDataResult &) {}
void publish(const FMDataSphericalResult &) {}
void publish(const FMDataDis &) {}
void shutdown() {}

} // namespace foxglove_viz
