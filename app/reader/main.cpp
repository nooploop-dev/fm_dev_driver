#include "foxglove_viz.hpp"
#include "main_common.hpp"
#include "reader_data_handle.hpp"
#include <CLI/CLI.hpp>
#include <chrono>
#include <filesystem>
#include <limits>
#include <memory>
#include <spdlog/details/os.h>
#include <spdlog/fmt/chrono.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string>
#include <system_error>
#include <vector>

namespace {
constexpr char LOG_DIR[] = "logs";
constexpr char LOG_FILE[] = "logs/logger.log"; // 与录制文件同在LOG_DIR下
constexpr std::size_t MAX_FILE_SIZE = 10 * 1024 * 1024;
constexpr std::size_t MAX_FILE_COUNT = 3;
constexpr int BAUD_RATE = 921600;
// Foxglove可视化与MCAP录制(仅在开启FM_BUILD_FOXGLOVE时生效)
constexpr char FOXGLOVE_HOST[] = "0.0.0.0";
constexpr uint16_t FOXGLOVE_PORT = 8765;
constexpr char FOXGLOVE_FRAME_ID[] = "fm_anchor_link";

// CLI::PositiveNumber按浮点校验, 报错信息带double范围, 这里按整数校验
CLI::Validator positive_int() {
  return CLI::Range(1, std::numeric_limits<int>::max()).description("POSITIVE");
}

// 录制文件路径，形如logs/fm_20260713_182432_123.mcap。
std::string record_file_path() {
  using namespace std::chrono;
  const auto now = system_clock::now();
  const auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
  // fmt::localtime已废弃, std::localtime非线程安全, 用spdlog的跨平台实现
  const std::tm tm =
      spdlog::details::os::localtime(system_clock::to_time_t(now));
  return fmt::format("{}/fm_{:%Y%m%d_%H%M%S}_{:03}.mcap", LOG_DIR, tm,
                     ms.count());
}
} // namespace

int main(int argc, char **argv) {
  CLI::App app{"Receive and parse data from an AOA device", APP_NAME};
  argv = app.ensure_utf8(argv);

  std::string port;
  int baud_rate = BAUD_RATE;
  uint16_t foxglove_port = FOXGLOVE_PORT;
  bool record = false;
  app.add_option("--port", port, "serial port, e.g. /dev/ttyUSB0")->required();
  app.add_option("--baudrate", baud_rate, "serial baud rate")
      ->check(positive_int())
      ->capture_default_str();
  app.add_option("--foxglove_port", foxglove_port, "Foxglove WebSocket port")
      ->check(CLI::Range(1, 65535))
      ->capture_default_str();
  app.add_flag("--record", record,
               "record data to logs/fm_<date>.mcap for later replay");
  CLI11_PARSE(app, argc, argv);

  {
    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    try {
      sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
          LOG_FILE, MAX_FILE_SIZE, MAX_FILE_COUNT));
    } catch (const spdlog::spdlog_ex &e) {
      spdlog::warn("Failed to create log file '{}': {}", LOG_FILE, e.what());
    }
    auto logger =
        std::make_shared<spdlog::logger>("", sinks.begin(), sinks.end());
    spdlog::set_default_logger(logger);
    spdlog::flush_every(std::chrono::seconds(1));
  }

  spdlog::info("Listening on {} at {} baud", port, baud_rate);
  if (foxglove_viz::init(FOXGLOVE_HOST, foxglove_port, FOXGLOVE_FRAME_ID)) {
    spdlog::info("Foxglove: connect to ws://{}:{}", FOXGLOVE_HOST,
                 foxglove_port);
  }
  if (record) {
    // MCAP写入器不会自动建目录, 而日志sink建目录失败时只告警不退出
    std::error_code ec;
    std::filesystem::create_directories(LOG_DIR, ec);
    const std::string file = record_file_path();
    if (foxglove_viz::record_start(file)) {
      spdlog::info("Record: writing to {}", file);
    }
  }

  FMParserFromDev parser;
  fm_parser_from_dev_init(&parser, data_handle::on_frame_begin,
                          data_handle::on_frame_msg, data_handle::on_frame_end);
  main_common_init(port, baud_rate, &parser);

  foxglove_viz::shutdown();
  spdlog::info("Exiting");
  spdlog::default_logger()->flush();
  return 0;
}
