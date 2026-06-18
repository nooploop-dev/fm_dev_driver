#include "serial_port.hpp"

SerialPort::SerialPort(asio::io_context &ioc, const std::string &port_name,
                       int baud_rate, data_ready_cb_f cb, int rx_buf_size)
    : port_(ioc), ioc_(ioc), port_name_(port_name), baud_rate_(baud_rate),
      data_ready_cb_(cb) {
  asio::error_code ec;
  port_.open(port_name_, ec);
  if (ec) {
    throw std::runtime_error(ec.message());
  }
  using namespace asio;
  port_.set_option(serial_port::baud_rate(baud_rate_));
  port_.set_option(serial_port::flow_control(serial_port::flow_control::none));
  port_.set_option(serial_port::parity(serial_port::parity::none));
  port_.set_option(serial_port::stop_bits(serial_port::stop_bits::one));
  port_.set_option(serial_port::character_size(8));
  read_buf_.resize(rx_buf_size);
  start_async_read();
}

void SerialPort::close() {
  asio::error_code ec;
  port_.cancel(ec);
  port_.close(ec);
}

// 可多线程访问
void SerialPort::write(const std::string &data) {
  asio::post(ioc_, [this, data]() {
    write_queue_.push_back(data);
    if (write_queue_.size() == 1) {
      start_async_write();
    }
  });
}

void SerialPort::start_async_read() {
  port_.async_read_some(
      asio::buffer(read_buf_),
      [this](asio::error_code ec, std::size_t bytes_transferred) {
        if (!ec) {
          std::string data(read_buf_.data(), bytes_transferred);
          if (data_ready_cb_) {
            data_ready_cb_(data);
          }
        } else {
          close();
          throw std::runtime_error(ec.message());
        }
        start_async_read();
      });
}

void SerialPort::start_async_write() {
  if (write_queue_.empty())
    return;

  asio::async_write(port_, asio::buffer(write_queue_.front()),
                    [this](asio::error_code ec, std::size_t bytes_transferred) {
                      (void)bytes_transferred;
                      if (!ec) {
                        write_queue_.pop_front();
                      } else {
                        close();
                        throw std::runtime_error(ec.message());
                      }
                      start_async_write();
                    });
}
