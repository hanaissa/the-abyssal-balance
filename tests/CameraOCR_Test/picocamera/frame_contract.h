#ifndef ABYSSAL_PICOCAMERA_FRAME_CONTRACT_H
#define ABYSSAL_PICOCAMERA_FRAME_CONTRACT_H
#include "pico_camera.h"
static inline bool frame_is_expected(const camera_fb_t *fb) {
    return fb && fb->buf && fb->format == PIXFORMAT_RGB565 &&
           fb->width == 160 && fb->height == 120 && fb->len == 38400;
}
#endif
