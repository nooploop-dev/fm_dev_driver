#include "foxglove_viz.hpp"
#include "main_common.hpp"
#include "reader_data_handle.hpp"
#include <CLI/CLI.hpp>
#include <limits>
#include <memory>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

namespace {
constexpr char LOG_FILE[] = "logs/logger.log";
constexpr std::size_t MAX_FILE_SIZE = 10 * 1024 * 1024;
constexpr std::size_t MAX_FILE_COUNT = 3;
constexpr int BAUD_RATE = 921600;
// Foxglove可视化(仅在开启FM_BUILD_FOXGLOVE时生效)
constexpr char FOXGLOVE_HOST[] = "0.0.0.0";
constexpr uint16_t FOXGLOVE_PORT = 8765;
constexpr char FOXGLOVE_FRAME_ID[] = "fm_anchor_link";

// CLI::PositiveNumber按浮点校验, 报错信息带double范围, 这里按整数校验
CLI::Validator positive_int() {
  return CLI::Range(1, std::numeric_limits<int>::max()).description("POSITIVE");
}
} // namespace

int main(int argc, char **argv) {
  CLI::App app{"Receive and parse data from an AOA device", APP_NAME};
  argv = app.ensure_utf8(argv);

  std::string port;
  int baud_rate = BAUD_RATE;
  uint16_t foxglove_port = FOXGLOVE_PORT;
  app.add_option("--port", port, "serial port, e.g. /dev/ttyUSB0")->required();
  app.add_option("--baudrate", baud_rate, "serial baud rate")
      ->check(positive_int())
      ->capture_default_str();
  app.add_option("--foxglove_port", foxglove_port, "Foxglove WebSocket port")
      ->check(CLI::Range(1, 65535))
      ->capture_default_str();
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

  FMParserFromDev parser;
  fm_parser_from_dev_init(&parser, data_handle::on_frame_begin,
                          data_handle::on_frame_msg, data_handle::on_frame_end);
  main_common_init(port, baud_rate, &parser);

  foxglove_viz::shutdown();
  spdlog::default_logger()->flush();
  return 0;
}
