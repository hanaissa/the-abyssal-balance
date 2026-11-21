#ifndef ABYSSAL_CAPTURE_SETUP_H
#define ABYSSAL_CAPTURE_SETUP_H
#include "sensor.h"

/* OV7670 bench-only settings. Keep the library sensor tables intact.
 * Read back every changed field: upstream set_reg returns a value, not status.
 * Register definitions are in vendor/PicoCamera/src/sensors/ov7670_regs.h. */
static inline bool capture_write_checked(sensor_t *s, int reg, int mask, int value) {
    s->set_reg(s, reg, mask, value);
    return s->get_reg(s, reg, mask) == (value & mask);
}
static inline unsigned capture_source_rows(unsigned start, unsigned stop, unsigned vref) {
    return ((stop << 2) | ((vref >> 2) & 3u)) -
           ((start << 2) | (vref & 3u));
}
static inline bool capture_setup(sensor_t *s, bool bars) {
    if (!s || !s->set_reg || !s->get_reg || !s->set_colorbar) { return false; }
    // 12..492 = 480 source rows, /4 = 120 output rows. Preserve VREF gain bits.
    if (!capture_write_checked(s, 0x19, 0xff, 12 >> 2) ||
        !capture_write_checked(s, 0x1a, 0xff, 492 >> 2) ||
        !capture_write_checked(s, 0x03, 0x0f, 0x00) ||
        // Upstream frame_control clears HREF[7:6] while writing window LSBs.
        // Restore the datasheet's 0x80 edge offset; keep the six window bits.
        // OV7670 rev 1.4 Table 5, HREF (0x32); Linux keeps these bits too.
        !capture_write_checked(s, 0x32, 0xc0, 0x80) ||
        // Internal divider four; rewrite after RGB565 and window configuration.
        !capture_write_checked(s, 0x11, 0x7f, 0x03) ||
        // Reverse PCLK, preserving VSYNC polarity and all other COM10 bits.
        !capture_write_checked(s, 0x15, 0x10, 0x10)) { return false; }
    // DSP bars avoid the scaler-pattern sync anomaly recorded on this module.
    if (s->set_colorbar(s, 0) != 0 ||
        s->get_reg(s, 0x70, 0x80) || s->get_reg(s, 0x71, 0x80)) { return false; }
    return capture_write_checked(s, 0x42, 0x08, bars ? 0x08 : 0);
}
#endif
