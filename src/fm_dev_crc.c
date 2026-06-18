#include "fm_dev_crc.h"
#include <string.h>

// 对应 https://www.lddgo.net/encrypt/crc 中的CRC-8 CRC-16-MODBUS
// CRC-32，来自https://github.com/whik/crc-lib-c

uint8_t fm_crc8_get(const void *data, int length) {
  const uint8_t *p = (const uint8_t *)data;
  uint8_t i;
  uint8_t crc = 0; // Initial value
  while (length--) {
    crc ^= *p++; // crc ^= *data; data++;
    for (i = 0; i < 8; i++) {
      if (crc & 0x80)
        crc = (crc << 1) ^ 0x07;
      else
        crc <<= 1;
    }
  }
  return crc;
}
uint16_t fm_crc16_get(const void *data, int length) {
  const uint8_t *p = (const uint8_t *)data;
  uint8_t i;
  uint16_t crc = 0xffff; // Initial value
  while (length--) {
    crc ^= *p++; // crc ^= *data; data++;
    for (i = 0; i < 8; ++i) {
      if (crc & 1)
        crc = (crc >> 1) ^ 0xA001; // 0xA001 = reverse 0x8005
      else
        crc = (crc >> 1);
    }
  }
  return crc;
}
uint32_t fm_crc32_get(const void *data, int length) {
  const uint8_t *p = (const uint8_t *)data;
  uint8_t i;
  uint32_t crc = 0xffffffff; // Initial value
  while (length--) {
    crc ^= *p++; // crc ^= *data; data++;
    for (i = 0; i < 8; ++i) {
      if (crc & 1)
        crc = (crc >> 1) ^ 0xEDB88320; // 0xEDB88320= reverse 0x04C11DB7
      else
        crc = (crc >> 1);
    }
  }
  return ~crc;
}

bool fm_crc8_verify(const void *data, int data_size) {
  const uint8_t *p = (const uint8_t *)data;
  return fm_crc8_get(data, data_size - 1) == p[data_size - 1];
}
void fm_crc8_update(void *data, int data_size) {
  uint8_t *p = (uint8_t *)data;
  p[data_size - 1] = fm_crc8_get(data, data_size - 1);
}

bool fm_crc16_verify(const void *data, int data_size) {
  const uint8_t *p = (const uint8_t *)data;
  uint16_t crc = fm_crc16_get(data, data_size - 2);
  return 0 == memcmp(&crc, p + data_size - 2, sizeof(crc));
}
void fm_crc16_update(void *data, int data_size) {
  uint8_t *p = (uint8_t *)data;
  uint16_t crc = fm_crc16_get(data, data_size - 2);
  memcpy(p + data_size - 2, &crc, sizeof(crc));
}

bool fm_crc32_verify(const void *data, int data_size) {
  const uint8_t *p = (const uint8_t *)data;
  uint32_t crc = fm_crc32_get(data, data_size - 4);
  return 0 == memcmp(&crc, p + data_size - 4, sizeof(crc));
}
void fm_crc32_update(void *data, int data_size) {
  uint8_t *p = (uint8_t *)data;
  uint32_t crc = fm_crc32_get(data, data_size - 4);
  memcpy(p + data_size - 4, &crc, sizeof(crc));
}
