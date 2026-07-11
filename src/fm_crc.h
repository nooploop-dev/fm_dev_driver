#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

uint8_t fm_crc8_get(const void *data, int data_size);
bool fm_crc8_verify(const void *data, int data_size);
void fm_crc8_update(void *data, int data_size);

uint16_t fm_crc16_get(const void *data, int data_size);
bool fm_crc16_verify(const void *data, int data_size);
void fm_crc16_update(void *data, int data_size);

uint32_t fm_crc32_get(const void *data, int data_size);
bool fm_crc32_verify(const void *data, int data_size);
void fm_crc32_update(void *data, int data_size);

#ifdef __cplusplus
}
#endif
