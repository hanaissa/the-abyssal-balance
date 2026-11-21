#ifndef ABYSSAL_CAMERA_H
#define ABYSSAL_CAMERA_H
#include "AppTypes.h"
#ifdef __cplusplus
extern "C" {
#endif

#define CAMERA_FRAME_WIDTH 320u
#define CAMERA_FRAME_HEIGHT 240u
#define CAMERA_FRAME_BYTES (CAMERA_FRAME_WIDTH * CAMERA_FRAME_HEIGHT * 2u)

typedef struct {
    const uint8_t *data; /* Raw RGB565 bytes, nominal sensor MSB-first order. */
    uint32_t length;
    uint16_t width;
    uint16_t height;
} CameraFrame;

typedef struct {
    uint32_t bytes_received;
    unsigned frame_starts;
    unsigned frame_ends;
    unsigned observed_rows;
    int capture_status; /* PIO: 0 success, -2 timeout, -3 short first frame. */
} CameraDiagnostics;

/* Call from a single control loop; bounded blocking hardware operations.
 * Uses existing AppConfig.h pins, shared I2C1 at 100 kHz, one 153600-byte
 * SRAM frame buffer, two PIO state machines and one DMA channel.
 * No USB terminal or PC is required. Repeated initialization is harmless.
 * APP_OK on success, APP_ERROR on hardware/configuration failure.
 * Host builds return APP_UNSUPPORTED for init/capture/color-bar selection. */
AppStatus camera_init(void);
/* Invalidates the previous frame even on failure. APP_NOT_READY before init.
 * A short first frame is rejected; data is never collected from a second. */
AppStatus camera_capture(void);
/* APP_NOT_READY without a valid capture; NULL -> APP_ERROR. The view belongs
 * to the driver, valid until capture, release, color-bar change or deinit. */
AppStatus camera_get_frame(CameraFrame *frame);
void camera_release_frame(void);
void camera_deinit(void);
/* Optional diagnostic pattern; disabled by default. Never enables UV. */
AppStatus camera_set_color_bars(bool enabled);
AppStatus camera_get_diagnostics(CameraDiagnostics *diagnostics);

#ifdef __cplusplus
}
#endif
#endif
