#pragma once

#include "fm_dev_driver.h"

namespace data_handle {
void on_frame_begin(fm_role_e wired_role, const uint8_t *wired_uid,
                    fm_frame_cnt_t frame_cnt);
void on_frame_msg(bool wired, fm_msg_id_t msg_id, const void *msg_payload,
                  int msg_payload_size);
void on_frame_end();
} // namespace data_handle
