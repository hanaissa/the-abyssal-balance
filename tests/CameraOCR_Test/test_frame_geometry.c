#include <stdio.h>
#include "frame_geometry.h"
#include "sync_filter.h"

int main(void) {
    const FrameMode vga = {640, 480, 4, 153600};
    const FrameMode qvga = {320, 240, 8, 76800};
    const FrameMode raw = {320, 240, 16, 153600};
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL: %s\n", #expr); return 1; } } while (0)
    CHECK(camera_frame_geometry_valid(&vga, 480, 153600, false));
    CHECK(camera_frame_geometry_valid(&vga, 477, 152640, false)); /* Observed board result. */
    CHECK(camera_frame_geometry_valid(&vga, 472, 151040, false)); /* Short-window limit. */
    CHECK(!camera_frame_geometry_valid(&vga, 471, 150720, false));
    CHECK(!camera_frame_geometry_valid(&vga, 477, 152639, false)); /* Partial row. */
    CHECK(!camera_frame_geometry_valid(&vga, 480, 152640, false)); /* Rows/bytes disagree. */
    CHECK(!camera_frame_geometry_valid(&vga, 477, 152640, true));
    CHECK(!camera_frame_geometry_valid(&vga, 481, 153920, false));
    CHECK(!camera_frame_geometry_valid(&vga, 0, 0, false));
    CHECK(camera_frame_geometry_valid(&qvga, 237, 75840, false));
    CHECK(camera_frame_geometry_valid(&raw, 237, 151680, false));
    CHECK(!camera_frame_geometry_valid(&raw, 237, 75840, false));
    CHECK(!camera_frame_geometry_valid(&qvga, 120, 38400, false)); /* Previous half-frame symptom. */
    SyncFilter sync = {0};
    CHECK(!sync_level_stable(&sync, true, 100, 50));
    CHECK(!sync_level_stable(&sync, true, 149, 50));
    CHECK(sync_level_stable(&sync, true, 150, 50));
    CHECK(!sync_level_stable(&sync, false, 151, 50));
    CHECK(!sync_level_stable(&sync, true, 152, 50)); /* Must start timing afresh. */
    CHECK(!sync_level_stable(&sync, false, 153, 50)); /* One-us spike rejected. */
    CHECK(!sync_level_stable(&sync, true, UINT32_MAX - 10, 50));
    CHECK(!sync_level_stable(&sync, true, 20, 50));
    CHECK(sync_level_stable(&sync, true, 40, 50)); /* Hardware timer wrap. */
    puts("PASS: geometry, half-frame rejection, sync spike rejection, fresh timing and timer wrap.");
    return 0;
}
