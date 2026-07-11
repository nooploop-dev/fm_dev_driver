#include "main_common.hpp"
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <spdlog/fmt/ranges.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>

namespace {

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
  if (argc != 3) {
    spdlog::info("Usage: {} <serial-port> <baudrate>", argv[0]);
    return 1;
  }
  const std::string port = argv[1];
  int baud_rate = std::atoi(argv[2]);
  if (baud_rate <= 0) {
    spdlog::error("Invalid baudrate: {}", argv[2]);
    return 2;
  }
  spdlog::info("Sending on {} at {} baud", port, baud_rate);
  std::thread sender(send_all);
  main_common_init(port, baud_rate, nullptr);
  sender.join();
  spdlog::default_logger()->flush();
  return 0;
}
