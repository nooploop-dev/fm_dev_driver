#include "main_common.hpp"
#include "serial_port.hpp"
#include <atomic>
#include <csignal>
#include <iostream>

namespace {
std::atomic_bool can_use_serial{true};
SerialPort *g_serial = nullptr;
fm_frame_cnt_t g_tx_cnt = 0;
} // namespace

void main_common_init(const std::string &serial_port_name, int baud_rate,
                      FMParserFromDev *parser) {
  asio::io_context ioc;
  can_use_serial = true;
  asio::signal_set signals(ioc, SIGINT, SIGTERM);
  signals.async_wait([&ioc](const asio::error_code &ec, int signal_number) {
    (void)signal_number;
    if (ec == asio::error::operation_aborted) {
      return;
    }
    can_use_serial = false;
    ioc.stop();
  });
  SerialPort::data_ready_cb_f data_ready;
  if (parser) {
    data_ready = [parser](const std::string &data) {
      fm_parser_from_dev_handle_data(parser, data.data(), (int)data.size());
    };
  }
  try {
    SerialPort serial_port(ioc, serial_port_name, baud_rate, data_ready);
    g_serial = &serial_port;
    ioc.run();
    can_use_serial = false;
    g_serial = nullptr;
  } catch (const std::exception &e) {
    std::cerr << "Failed to open serial port, it may be occupied. Error: "
              << e.what() << std::endl;
    exit(1);
  }
}

void serial_try_send_data(const void *data, int data_size) {
  if (can_use_serial && g_serial && data_size > 0) {
    g_serial->write(
        std::string(reinterpret_cast<const char *>(data), data_size));
  }
}

void main_common_send_msg(fm_connect_type_e connect_type, fm_msg_id_t msg_id,
                          const void *msg_payload, int msg_payload_size) {
  (void)msg_payload_size; // fm_prepare_msg_to_dev 由msg_id推断负载大小
  uint8_t data[FM_FRAME_SIZE_MAX];
  int data_size = fm_prepare_msg_to_dev(connect_type, g_tx_cnt++, msg_id,
                                        msg_payload, data, sizeof(data));
  serial_try_send_data(data, data_size);
}
