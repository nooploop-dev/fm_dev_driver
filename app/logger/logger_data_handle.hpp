#pragma once

#include "fm_dev_driver.h"

namespace data_handle {
void on_msg(fm_connect_type_e connect_type, fm_role_e role, const uint8_t *uid,
            fm_frame_cnt_t frame_cnt, fm_msg_id_t msg_id,
            const void *msg_payload, int msg_payload_size);
}
