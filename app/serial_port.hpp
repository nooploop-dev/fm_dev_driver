#pragma once

#include <asio.hpp>
#include <deque>
#include <functional>
#include <string>

class SerialPort {
public:
  using data_ready_cb_f = std::function<void(const std::string &data)>;
  SerialPort(asio::io_context &ioc, const std::string &port_name, int baud_rate,
             data_ready_cb_f cb, int rx_buf_size = 4096);

  ~SerialPort() { close(); }

  void write(const std::string &data);

private:
  void close();
  void start_async_read();

  void start_async_write();

  asio::serial_port port_;
  asio::io_context &ioc_;
  std::string port_name_;
  int baud_rate_;
  data_ready_cb_f data_ready_cb_;
  std::string read_buf_;
  std::deque<std::string> write_queue_;
};
