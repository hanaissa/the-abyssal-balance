#ifndef ABYSSAL_CAMERA_CRC32_H
#define ABYSSAL_CAMERA_CRC32_H
#include <stdint.h>
#include <stddef.h>
static inline uint32_t image_crc32(const uint8_t *data, size_t length) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320u : 0);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}
#endif
