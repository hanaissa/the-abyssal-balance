#ifndef ABYSSAL_CAPTURE_GEOMETRY_H
#define ABYSSAL_CAPTURE_GEOMETRY_H
#include <stdbool.h>

typedef struct { unsigned width, height, bits, bytes; } FrameMode;

/* Diagnostic-only allowance for a few rows outside the sampled VSYNC window.
 * Report the actual height; never pad, stretch or continue into another frame.
 * The byte total must still describe complete rows of the requested width. */
static inline bool camera_frame_geometry_valid(const FrameMode *mode,
                                               unsigned lines, unsigned bytes,
                                               bool overflow) {
    return !overflow && lines > 0 && lines <= mode->height &&
           mode->height - lines <= 8 &&
           bytes == lines * mode->width * mode->bits / 8;
}
#endif
