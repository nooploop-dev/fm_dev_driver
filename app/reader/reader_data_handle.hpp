#pragma once

#include "fm_driver_for_user.h"

namespace data_handle {
void on_frame_begin(fm_role_e wired_role, const uint8_t *wired_uid,
                    fm_frame_cnt_t frame_cnt);
void on_frame_msg(fm_connect_type_e connect_type, fm_msg_id_t msg_id,
                  const void *msg_payload, int msg_payload_size);
void on_frame_end();
} // namespace data_handle
