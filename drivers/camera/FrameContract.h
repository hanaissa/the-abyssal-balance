#ifndef ABYSSAL_PICOCAMERA_FRAME_CONTRACT_H
#define ABYSSAL_PICOCAMERA_FRAME_CONTRACT_H
#include "pico_camera.h"
#include "Camera.h"
#define CAPTURE_WIDTH CAMERA_FRAME_WIDTH
#define CAPTURE_HEIGHT CAMERA_FRAME_HEIGHT
#define CAPTURE_BYTES (CAPTURE_WIDTH * CAPTURE_HEIGHT * 2u)
static inline bool frame_is_expected(const camera_fb_t *fb) {
    return fb && fb->buf && fb->format == PIXFORMAT_RGB565 &&
           fb->width == CAPTURE_WIDTH && fb->height == CAPTURE_HEIGHT && fb->len == CAPTURE_BYTES;
}
#endif
