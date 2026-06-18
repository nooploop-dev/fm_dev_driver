#include "fm_dev_frame.h"
#include "fm_dev_crc.h"
#include <string.h>

const FMFrameLayout fm_frame_dev_to_user_layout = {
    FM_FRAME_DEV_TO_USER_SIZE_MIN,
    (int)offsetof(FMFrameDevToUser, payload_size),
    FM_FRAME_DEV_TO_USER_PAYLOAD_SIZE_MAX,
};
const FMFrameLayout fm_frame_user_to_dev_layout = {
    FM_FRAME_USER_TO_DEV_SIZE_MIN,
    (int)offsetof(FMFrameUserToDev, payload_size),
    FM_FRAME_USER_TO_DEV_PAYLOAD_SIZE_MAX,
};

// 从缓冲区中尽可能多地提取完整帧，并把剩余(未消费)数据前移到缓冲区起始
static void fm_frame_buffer_extract(uint8_t *buffer, int *index_begin,
                                    int *index_end, const FMFrameLayout *layout,
                                    fm_frame_cb_f cb, void *arg) {
  while (*index_end - *index_begin >= layout->size_min) {
    const uint8_t *frame = buffer + *index_begin;
    if (frame[0] != FM_FRAME_SOF) {
      *index_begin += 1;
      continue;
    }
    fm_frame_payload_size_t payload_size;
    memcpy(&payload_size, frame + layout->payload_size_offset,
           sizeof(payload_size));
    if ((int)payload_size > layout->payload_size_max) {
      *index_begin += 1;
      continue;
    }
    int frame_size = layout->size_min + (int)payload_size;
    if (*index_end - *index_begin < frame_size) {
      break; // 帧尚未接收完整，等待更多数据
    }
    if (!fm_crc16_verify(frame, frame_size)) {
      *index_begin += 1;
      continue;
    }
    cb(arg, frame, frame_size);
    *index_begin += frame_size;
  }
  if (*index_begin > 0) {
    memmove(buffer, buffer + *index_begin, *index_end - *index_begin);
    *index_end -= *index_begin;
    *index_begin = 0;
  }
}

void fm_frame_buffer_feed(uint8_t *buffer, int *index_begin, int *index_end,
                          const FMFrameLayout *layout, const void *data,
                          int data_size, fm_frame_cb_f cb, void *arg) {
  const uint8_t *p = (const uint8_t *)data;
  int offset = 0;
  while (offset < data_size) {
    // 按缓冲区剩余容量分块写入，写一块提取一块
    int remain_buf = FM_FRAME_SIZE_MAX - *index_end;
    int remain_in = data_size - offset;
    int size = remain_buf < remain_in ? remain_buf : remain_in;
    memcpy(buffer + *index_end, p + offset, size);
    *index_end += size;
    offset += size;
    fm_frame_buffer_extract(buffer, index_begin, index_end, layout, cb, arg);
  }
}
