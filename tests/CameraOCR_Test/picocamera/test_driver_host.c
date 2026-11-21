#include <stdio.h>
#include "Camera.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL: %s\n", #x); return 1; } } while (0)
int main(void) {
    CameraFrame frame = {.data=(const uint8_t *)1, .length=123, .width=1, .height=1};
    CameraDiagnostics diagnostics;
    CHECK(camera_init() == APP_UNSUPPORTED);
    CHECK(camera_capture() == APP_UNSUPPORTED);
    CHECK(camera_set_color_bars(true) == APP_UNSUPPORTED);
    CHECK(camera_get_frame(NULL) == APP_ERROR);
    CHECK(camera_get_frame(&frame) == APP_NOT_READY);
    CHECK(!frame.data && !frame.length && !frame.width && !frame.height);
    CHECK(camera_get_diagnostics(NULL) == APP_ERROR);
    CHECK(camera_get_diagnostics(&diagnostics) == APP_NOT_READY);
    camera_release_frame(); camera_deinit(); camera_deinit();
    CHECK(camera_get_frame(&frame) == APP_NOT_READY);
    puts("PASS production host API: unsupported hardware, cleared outputs, NULL, release/deinit.");
    return 0;
}
