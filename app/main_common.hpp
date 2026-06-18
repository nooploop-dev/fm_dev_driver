#pragma once

#include "fm_dev_driver.h"
#include <string>

// 打开串口并驱动parser处理收到的数据，阻塞直到退出
void main_common_init(const std::string &serial_port_name, int baud_rate,
                      FMParserFromDev *parser);

// 向设备发送一条消息(内部维护帧计数并通过串口发出)
void main_common_send_msg(fm_connect_type_e connect_type, fm_msg_id_t msg_id,
                          const void *msg_payload, int msg_payload_size);
