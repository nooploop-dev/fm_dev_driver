#include "foxglove_viz.hpp"

#include <chrono>
#include <cstddef>
#include <optional>
#include <spdlog/spdlog.h>

#include <foxglove/channel.hpp>
#include <foxglove/error.hpp>
#include <foxglove/mcap.hpp>
#include <foxglove/messages.hpp>
#include <foxglove/websocket.hpp>

namespace {

namespace fgm = foxglove::messages;

std::optional<foxglove::WebSocketServer> s_server;
std::optional<foxglove::McapWriter> s_writer;
std::optional<fgm::FrameTransformChannel> s_tf_ch;
std::optional<fgm::PoseInFrameChannel> s_pose_ch;
std::optional<fgm::SceneUpdateChannel> s_scene_ch;
std::optional<foxglove::RawChannel> s_result_ch;
std::optional<foxglove::RawChannel> s_spherical_ch;
std::optional<foxglove::RawChannel> s_dis_ch;
std::string s_frame_id;

// Foxglove 3D面板需要一个根坐标系，定位结果所在的frame挂在它下面
constexpr char ROOT_FRAME[] = "world";

fgm::Timestamp now_ts() {
  auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count();
  fgm::Timestamp ts;
  ts.sec = uint32_t(ns / 1000000000);
  ts.nsec = uint32_t(ns % 1000000000);
  return ts;
}

// 供Foxglove的Plot面板绘制曲线用的JSON schema
const std::string RESULT_SCHEMA = R"({
  "type": "object",
  "properties": {
    "local_time": {"type": "number"},
    "cnt": {"type": "number"},
    "pos_x": {"type": "number"}, "pos_y": {"type": "number"}, "pos_z": {"type": "number"},
    "vel_x": {"type": "number"}, "vel_y": {"type": "number"}, "vel_z": {"type": "number"},
    "pos_noise_x": {"type": "number"}, "pos_noise_y": {"type": "number"}, "pos_noise_z": {"type": "number"},
    "vel_noise_x": {"type": "number"}, "vel_noise_y": {"type": "number"}, "vel_noise_z": {"type": "number"}
  }
})";

const std::string SPHERICAL_SCHEMA = R"({
  "type": "object",
  "properties": {
    "local_time": {"type": "number"},
    "cnt": {"type": "number"},
    "dis": {"type": "number"},
    "azimuth": {"type": "number"},
    "elevation": {"type": "number"}
  }
})";

const std::string DIS_SCHEMA = R"({
  "type": "object",
  "properties": {
    "local_time": {"type": "number"},
    "cnt": {"type": "number"},
    "dis": {"type": "number"},
    "rx_rate": {"type": "number"}
  }
})";

foxglove::Schema json_schema(const std::string &name, const std::string &text) {
  foxglove::Schema schema;
  schema.name = name;
  schema.encoding = "jsonschema";
  schema.data = reinterpret_cast<const std::byte *>(text.data());
  schema.data_len = text.size();
  return schema;
}

void log_json(std::optional<foxglove::RawChannel> &ch, const std::string &json) {
  if (!ch) {
    return;
  }
  ch->log(reinterpret_cast<const std::byte *>(json.data()), json.size());
}

// 创建通道，失败时打日志并返回空。通道类型由FoxgloveResult<T>推导
template <typename T>
std::optional<T> take(foxglove::FoxgloveResult<T> &&result, const char *topic) {
  if (!result.has_value()) {
    spdlog::warn("Foxglove: failed to create channel '{}': {}", topic,
                 foxglove::strerror(result.error()));
    return std::nullopt;
  }
  return std::optional<T>(std::move(result.value()));
}

// 通道只创建一次，WebSocket服务与MCAP录制都是它的下游(sink)，各自可独立启停
void ensure_channels() {
  static bool created = false;
  if (created) {
    return;
  }
  created = true;

  s_tf_ch = take(fgm::FrameTransformChannel::create("/tf"), "/tf");
  s_pose_ch =
      take(fgm::PoseInFrameChannel::create("/result_pose"), "/result_pose");
  s_scene_ch =
      take(fgm::SceneUpdateChannel::create("/result_scene"), "/result_scene");
  s_result_ch = take(foxglove::RawChannel::create(
                         "/result", "json",
                         json_schema("fm.Result", RESULT_SCHEMA)),
                     "/result");
  s_spherical_ch =
      take(foxglove::RawChannel::create(
               "/spherical_result", "json",
               json_schema("fm.SphericalResult", SPHERICAL_SCHEMA)),
           "/spherical_result");
  s_dis_ch = take(
      foxglove::RawChannel::create("/dis", "json",
                                   json_schema("fm.Dis", DIS_SCHEMA)),
      "/dis");
}

} // namespace

namespace foxglove_viz {

bool init(const std::string &host, uint16_t port, const std::string &frame_id) {
  s_frame_id = frame_id;
  ensure_channels();

  foxglove::WebSocketServerOptions options;
  options.name = "fm_reader";
  options.host = host;
  options.port = port;

  auto server = foxglove::WebSocketServer::create(std::move(options));
  if (!server.has_value()) {
    spdlog::error("Foxglove: failed to start server on {}:{}: {}", host, port,
                  foxglove::strerror(server.error()));
    return false;
  }
  s_server.emplace(std::move(server.value()));
  return true;
}

bool record_start(const std::string &file_path) {
  ensure_channels();

  foxglove::McapWriterOptions options;
  options.path = file_path;

  auto writer = foxglove::McapWriter::create(options);
  if (!writer.has_value()) {
    spdlog::error("Record: failed to create '{}': {}", file_path,
                  foxglove::strerror(writer.error()));
    return false;
  }
  s_writer.emplace(std::move(writer.value()));
  return true;
}

void publish(const FMDataResult &data) {
  auto ts = now_ts();

  // 根坐标系 -> 基站坐标系。3D面板需要这个TF，定位点才有落脚的frame
  if (s_tf_ch) {
    fgm::FrameTransform tf;
    tf.timestamp = ts;
    tf.parent_frame_id = ROOT_FRAME;
    tf.child_frame_id = s_frame_id;
    tf.translation = fgm::Vector3{0, 0, 0};
    tf.rotation = fgm::Quaternion{0, 0, 0, 1};
    s_tf_ch->log(tf);
  }

  // 定位点。设备不测姿态，姿态填单位四元数
  if (s_pose_ch) {
    fgm::PoseInFrame pose;
    pose.timestamp = ts;
    pose.frame_id = s_frame_id;
    pose.pose = fgm::Pose{fgm::Vector3{data.pos[0], data.pos[1], data.pos[2]},
                          fgm::Quaternion{0, 0, 0, 1}};
    s_pose_ch->log(pose);
  }

  // 不确定性球(直径取2σ，与rviz的协方差椭球对应)+ 速度矢量线段
  if (s_scene_ch) {
    fgm::SpherePrimitive sphere;
    sphere.pose = fgm::Pose{fgm::Vector3{data.pos[0], data.pos[1], data.pos[2]},
                            fgm::Quaternion{0, 0, 0, 1}};
    sphere.size = fgm::Vector3{2.0 * data.pos_noise[0], 2.0 * data.pos_noise[1],
                               2.0 * data.pos_noise[2]};
    sphere.color = fgm::Color{0.8, 0.2, 0.8, 0.3};

    fgm::LinePrimitive vel;
    vel.type = fgm::LinePrimitive::LineType::LINE_STRIP;
    vel.thickness = 2.0;
    vel.scale_invariant = true;
    vel.color = fgm::Color{1.0, 0.9, 0.2, 1.0};
    vel.points = {
        fgm::Point3{data.pos[0], data.pos[1], data.pos[2]},
        fgm::Point3{data.pos[0] + data.vel[0], data.pos[1] + data.vel[1],
                    data.pos[2] + data.vel[2]},
    };

    fgm::SceneEntity entity;
    entity.timestamp = ts;
    entity.frame_id = s_frame_id;
    entity.id = "result";
    entity.spheres = {sphere};
    entity.lines = {vel};

    fgm::SceneUpdate update;
    update.entities = {entity};
    s_scene_ch->log(update);
  }

  log_json(s_result_ch,
           fmt::format(R"({{"local_time":{},"cnt":{},)"
                       R"("pos_x":{},"pos_y":{},"pos_z":{},)"
                       R"("vel_x":{},"vel_y":{},"vel_z":{},)"
                       R"("pos_noise_x":{},"pos_noise_y":{},"pos_noise_z":{},)"
                       R"("vel_noise_x":{},"vel_noise_y":{},"vel_noise_z":{}}})",
                       data.local_time, data.cnt, data.pos[0], data.pos[1],
                       data.pos[2], data.vel[0], data.vel[1], data.vel[2],
                       data.pos_noise[0], data.pos_noise[1], data.pos_noise[2],
                       data.vel_noise[0], data.vel_noise[1],
                       data.vel_noise[2]));
}

void publish(const FMDataSphericalResult &data) {
  log_json(s_spherical_ch,
           fmt::format(R"({{"local_time":{},"cnt":{},"dis":{},)"
                       R"("azimuth":{},"elevation":{}}})",
                       data.local_time, data.cnt, data.dis, data.azimuth,
                       data.elevation));
}

void publish(const FMDataDis &data) {
  log_json(s_dis_ch,
           fmt::format(
               R"({{"local_time":{},"cnt":{},"dis":{},"rx_rate":{}}})",
               data.local_time, data.cnt, data.dis, data.rx_rate));
}

void shutdown() {
  if (s_server) {
    s_server->stop();
    s_server.reset();
  }
  if (s_writer) {
    // 不close的话MCAP缺少索引/统计信息，Foxglove无法正常打开
    auto err = s_writer->close();
    if (err != foxglove::FoxgloveError::Ok) {
      spdlog::error("Record: failed to close file: {}",
                    foxglove::strerror(err));
    }
    s_writer.reset();
  }
}

} // namespace foxglove_viz
