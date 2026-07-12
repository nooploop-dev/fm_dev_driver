// FM_BUILD_FOXGLOVE 关闭时链接本文件，reader 的行为与集成 Foxglove 前完全一致。
#include "foxglove_viz.hpp"

namespace foxglove_viz {

bool init(const std::string &, uint16_t, const std::string &) { return false; }
void publish(const FMDataResult &) {}
void publish(const FMDataSphericalResult &) {}
void publish(const FMDataDis &) {}
void shutdown() {}

} // namespace foxglove_viz
