#include <stdio.h>
#include "frame_contract.h"
#include "crc32.h"
int main(void) {
    uint8_t placeholder = 0;
    camera_fb_t fb = {.buf=&placeholder, .width=160, .height=120,
                      .len=38400, .format=PIXFORMAT_RGB565};
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL: %s\n", #x); return 1; } } while(0)
    CHECK(frame_is_expected(&fb));
    CHECK(!frame_is_expected(NULL));
    CHECK(image_crc32((const uint8_t *)"123456789", 9) == 0xCBF43926u);
    CHECK(image_crc32(&placeholder, 0) == 0);
    fb.len = 38399; CHECK(!frame_is_expected(&fb));
    fb.len = 38400; fb.height = 119; CHECK(!frame_is_expected(&fb));
    fb.height = 120; fb.format = PIXFORMAT_YUV422; CHECK(!frame_is_expected(&fb));
    fb.format = PIXFORMAT_RGB565; fb.buf = NULL; CHECK(!frame_is_expected(&fb));
    puts("PASS frame metadata: valid RGB565, truncated payload, wrong dimensions/format, NULL.");
    return 0;
}
