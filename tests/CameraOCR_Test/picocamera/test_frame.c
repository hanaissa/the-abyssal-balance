#include <stdio.h>
#include "frame_contract.h"
#include "crc32.h"
#include "capture_setup.h"
static unsigned char registers[256];
static int failed_register = -1;
static int read_reg(sensor_t *s, int reg, int mask) { (void)s; return registers[reg] & mask; }
static int write_reg(sensor_t *s, int reg, int mask, int value) {
    (void)s;
    if (reg != failed_register) { registers[reg] = (registers[reg] & ~mask) | (value & mask); }
    return value;
}
static int colorbar(sensor_t *s, int enabled) {
    (void)enabled;
    write_reg(s, 0x70, 0x80, 0); write_reg(s, 0x71, 0x80, 0); return 0;
}
int main(void) {
    uint8_t placeholder = 0;
    camera_fb_t fb = {.buf=&placeholder, .width=320, .height=240,
                      .len=153600, .format=PIXFORMAT_RGB565};
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL: %s\n", #x); return 1; } } while(0)
    CHECK(frame_is_expected(&fb));
    CHECK(!frame_is_expected(NULL));
    CHECK(image_crc32((const uint8_t *)"123456789", 9) == 0xCBF43926u);
    CHECK(image_crc32(&placeholder, 0) == 0);
    fb.len = 153599; CHECK(!frame_is_expected(&fb));
    fb.len = 38400; CHECK(!frame_is_expected(&fb));
    fb.len = 153600; fb.height = 239; CHECK(!frame_is_expected(&fb));
    fb.height = 240; fb.format = PIXFORMAT_YUV422; CHECK(!frame_is_expected(&fb));
    fb.format = PIXFORMAT_RGB565; fb.buf = NULL; CHECK(!frame_is_expected(&fb));
    sensor_t sensor = {.set_reg=write_reg, .get_reg=read_reg, .set_colorbar=colorbar};
    CHECK(capture_source_rows(12 >> 2, 490 >> 2, 8) == 478);
    registers[0x03] = 0xf8; registers[0x15] = 0x32; registers[0xa2] = 0x82;
    registers[0x32] = 0x36; // upstream window write has erased edge-offset bits
    CHECK(capture_setup(&sensor, true));
    CHECK(capture_source_rows(registers[0x19], registers[0x1a], registers[0x03]) == 480);
    CHECK(registers[0x03] == 0xf0);  // preserve gain bits
    CHECK(registers[0x15] == 0x22);  // normal PCLK, preserve sync/blanking bits
    CHECK(registers[0xa2] == 0x80);  // zero delay, preserve reserved high bit
    CHECK(registers[0x32] == 0xb6);  // restore offset without changing window LSBs
    CHECK(registers[0x11] == 3 && registers[0x42] == 8);
    CHECK(capture_setup(&sensor, false) && registers[0x42] == 0);
    registers[0x1a] = 0; failed_register = 0x1a;
    CHECK(!capture_setup(&sensor, false));  // ACK/readback failure must reject
    failed_register = 0x32; registers[0x1a] = 492 >> 2; registers[0x32] = 0x36;
    CHECK(!capture_setup(&sensor, false)); // do not capture if offset restore failed
    failed_register = 0xa2; registers[0x32] = 0xb6; registers[0xa2] = 2;
    CHECK(!capture_setup(&sensor, false)); // a failed delay write must reject capture
    failed_register = -1; registers[0x32] = 0x09;
    CHECK(capture_setup(&sensor, false) && registers[0x32] == 0x89);
    CHECK(!capture_setup(NULL, false));
    puts("PASS frame metadata: valid RGB565, truncated payload, wrong dimensions/format, NULL.");
    return 0;
}
