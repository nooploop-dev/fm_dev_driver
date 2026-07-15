#include "main_common.hpp"
#include <CLI/CLI.hpp>
#include <chrono>
#include <csignal>
#include <cstring>
#include <limits>
#include <memory>
#include <spdlog/fmt/ranges.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>

namespace {

constexpr int BAUD_RATE = 921600;

// CLI::PositiveNumber按浮点校验, 报错信息带double范围, 这里按整数校验
CLI::Validator positive_int() {
  return CLI::Range(1, std::numeric_limits<int>::max()).description("POSITIVE");
}

void send_msg(fm_connect_type_e connect_type, fm_msg_id_t msg_id,
              const void *msg_payload, const char *desc) {
  static fm_frame_cnt_t tx_cnt;
  uint8_t frame[FM_FRAME_SIZE_MAX];
  int frame_size = fm_prepare_msg_to_dev(connect_type, tx_cnt++, msg_id,
                                         msg_payload, frame, sizeof(frame));
  if (frame_size <= 0) {
    spdlog::error("{}: prepare frame failed", desc);
    return;
  }
  serial_try_send_data(frame, frame_size);
  spdlog::info("{}:bytes={}, data={:02X}", desc, frame_size,
               fmt::join(frame, frame + frame_size, ""));
}

void fill_payload(uint8_t *payload, uint8_t *payload_size, const char *text) {
  *payload_size = (uint8_t)strlen(text);
  memcpy(payload, text, *payload_size);
}

void send_all() {
  // 等待main_common_init打开串口并启动io_context
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  {
    FMDataEcho echo;
    fill_payload(echo.payload, &echo.payload_size, "echo wired");
    send_msg(FM_WIRED, FM_MSG_ECHO, &echo, "FM_MSG_ECHO(WIRED)");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  {
    FMDataEcho echo;
    fill_payload(echo.payload, &echo.payload_size, "echo wireless");
    send_msg(FM_WIRELESS, FM_MSG_ECHO, &echo, "FM_MSG_ECHO(WIRELESS)");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  {
    FMDataDataUserToUser user_data;
    fill_payload(user_data.payload, &user_data.payload_size, "user to user");
    send_msg(FM_WIRED, FM_MSG_DATA_USER_TO_USER, &user_data,
             "FM_MSG_DATA_USER_TO_USER(WIRED)");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  std::raise(SIGINT);
}

} // namespace

int main(int argc, char **argv) {
  CLI::App app{"Assemble and send messages to an AOA device", APP_NAME};
  argv = app.ensure_utf8(argv);

  std::string port;
  int baud_rate = BAUD_RATE;
  app.add_option("--port", port, "serial port, e.g. /dev/ttyUSB0")->required();
  app.add_option("--baudrate", baud_rate, "serial baud rate")
      ->check(positive_int())
      ->capture_default_str();
  CLI11_PARSE(app, argc, argv);

  spdlog::info("Sending on {} at {} baud", port, baud_rate);
  std::thread sender(send_all);
  main_common_init(port, baud_rate, nullptr);
  sender.join();
  spdlog::default_logger()->flush();
  return 0;
}
