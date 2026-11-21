/* USB preview and hardware API checks. The capture implementation is entirely
 * in drivers/camera; the station does not need this executable or a computer. */
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "Camera.h"
#include "OCR.h"
#include "crc32.h"

static void finish(void) {
    camera_deinit();
    stdio_flush();
    for (;;) { sleep_ms(100); }
}
static void report(void) {
    CameraDiagnostics diagnostics;
    camera_get_diagnostics(&diagnostics);
    printf("CAPTURE_DIAG received=%lu/%u status=%d starts=%u ends=%u HREF_rows=%u\n",
           (unsigned long)diagnostics.bytes_received, CAMERA_FRAME_BYTES,
           diagnostics.capture_status, diagnostics.frame_starts,
           diagnostics.frame_ends, diagnostics.observed_rows);
}
int main(void) {
    stdio_init_all();
    while (!stdio_usb_connected()) { sleep_ms(100); }
    sleep_ms(500);
    puts("Camera driver preview: native 320x240 RGB565; external pump power OFF.");
    puts("Capture implementation: drivers/camera/Camera.c; normal PCLK, scaler delay zero.");
    puts("OCR revision 2: 24x24 glyph model, joined-letter splitting, optional crop.");
    puts("Check module supply/logic, RESET HIGH, PWDN LOW, shared I2C pull-ups at 3.3V.");
    puts("Send c after checking wiring for scene, or t for internal color bars. One RGB565 frame.");
    int command;
    do { command = getchar_timeout_us(10000); } while (command != 'c' && command != 't');
    OCRConfig ocr_settings={.single_line=true};
    char options[80]; unsigned options_length=0;
    uint32_t option_start=time_us_32();
    while ((uint32_t)(time_us_32()-option_start)<200000u && options_length+1<sizeof options) {
        int ch=getchar_timeout_us(1000);
        if (ch=='\n') break;
        if (ch>=0 && ch<128) options[options_length++]=(char)ch;
    }
    options[options_length]=0;
    if (options_length) {
        unsigned x,y,w,h; char extra;
        if (sscanf(options," %u %u %u %u %c",&x,&y,&w,&h,&extra)!=4 ||
            x>=320 || y>=240 || w<3 || h<3 || w>320-x || h>240-y) {
            puts("ERROR invalid OCR crop."); finish();
        }
        ocr_settings=(OCRConfig){(uint16_t)x,(uint16_t)y,(uint16_t)w,(uint16_t)h,0,false,true};
    }
    CameraFrame frame;
    if (camera_capture() != APP_NOT_READY || camera_get_frame(NULL) != APP_ERROR ||
        camera_get_frame(&frame) != APP_NOT_READY || frame.data != NULL) {
        puts("ERROR camera API boundary check failed."); finish();
    }
    puts("Driver checks passed: capture before init and NULL/empty frame rejection.");
    if (camera_init() != APP_OK || camera_init() != APP_OK) {
        puts("ERROR camera driver initialization failed."); finish();
    }
    if (command == 't' && camera_set_color_bars(true) != APP_OK) {
        puts("ERROR enabling internal color bars."); finish();
    }
    puts(command == 't' ? "MODE internal DSP bars; NOT a scene photo." : "MODE actual scene.");
    // Exercise repeated capture and explicit release with one allocation/init.
    for (unsigned attempt = 0; attempt < 2; ++attempt) {
        stdio_flush();
        AppStatus status = camera_capture();
        report();
        if (status != APP_OK || camera_get_frame(&frame) != APP_OK) {
            puts("ERROR camera driver capture failed; no image saved."); finish();
        }
        if (attempt == 0) {
            camera_release_frame();
            if (camera_get_frame(&frame) != APP_NOT_READY || frame.data != NULL) {
                puts("ERROR released frame remained accessible."); finish();
            }
        }
    }
    puts("Driver repeat-capture and release checks passed.");
    if (command=='c') {
        char text[256]; OCRDiagnostics diagnostics;
        ocr_init();
        if (ocr_configure(&ocr_settings)!=APP_OK) { puts("ERROR invalid OCR settings."); finish(); }
        printf("OCR_ROI x=%u y=%u width=%u height=%u\n",ocr_settings.x,ocr_settings.y,
               ocr_settings.width,ocr_settings.height);
        puts("OCR_RUNNING experimental block-handwriting recognition on Pico.");
        stdio_flush();
        AppStatus status=ocr_read_text(text,sizeof text);
        ocr_get_diagnostics(&diagnostics);
        printf("OCR_STATUS %d glyphs=%u rejected=%u confidence=%u\n",status,
               diagnostics.glyphs,diagnostics.rejected,diagnostics.minimum_confidence);
        if (status==APP_OK) printf("OCR_TEXT %s\n",text);
        else puts("OCR could not confidently read all glyphs. No text accepted.");
    }
    printf("ABYSSAL_RGB565 1 %u %u %lu %08lX\n", frame.width, frame.height,
           (unsigned long)frame.length, (unsigned long)image_crc32(frame.data, frame.length));
    for (size_t offset = 0; offset < frame.length && stdio_usb_connected(); offset += 256) {
        size_t count = frame.length - offset;
        if (count > 256) { count = 256; }
        stdio_put_string((const char *)frame.data + offset, (int)count, false, false);
    }
    static const char trailer[] = "\nEND_RGB565\n";
    stdio_put_string(trailer, sizeof trailer - 1, false, false);
    camera_deinit();
    if (camera_get_frame(&frame) != APP_NOT_READY || frame.data != NULL) {
        puts("ERROR deinitialized frame remained accessible.");
    }
    puts("Driver test finished. Reconnect USB normally to repeat.");
    finish();
}
