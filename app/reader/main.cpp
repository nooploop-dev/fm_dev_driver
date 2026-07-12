#include "foxglove_viz.hpp"
#include "main_common.hpp"
#include "reader_data_handle.hpp"
#include <cstdlib>
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
// Foxglove可视化(仅在开启FM_BUILD_FOXGLOVE时生效)
constexpr char FOXGLOVE_HOST[] = "0.0.0.0";
constexpr uint16_t FOXGLOVE_PORT = 8765;
constexpr char FOXGLOVE_FRAME_ID[] = "fm_anchor_link";
} // namespace

int main(int argc, char **argv) {
  if (argc < 3 || argc > 4) {
    spdlog::info("Usage: {} <serial-port> <baudrate> [foxglove-port]", argv[0]);
    return 1;
  }
  const std::string port = argv[1];
  int baud_rate = std::atoi(argv[2]);
  if (baud_rate <= 0) {
    spdlog::error("Invalid baudrate: {}", argv[2]);
    return 2;
  }
  uint16_t foxglove_port = FOXGLOVE_PORT;
  if (argc == 4) {
    int p = std::atoi(argv[3]);
    if (p <= 0 || p > 65535) {
      spdlog::error("Invalid foxglove port: {}", argv[3]);
      return 3;
    }
    foxglove_port = uint16_t(p);
  }

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
