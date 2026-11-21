#ifndef ABYSSAL_OCR_H
#define ABYSSAL_OCR_H
#include "AppTypes.h"
#include "Camera.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t x, y, width, height; /* Region after rotation; all zero = full frame. */
    uint8_t rotation;            /* Clockwise quarter turns, 0..3. */
    bool light_text;             /* false: dark ink; true: bright UV-revealed ink. */
    bool single_line;            /* Select the line with strongest glyph evidence. */
} OCRConfig;
typedef struct {
    uint16_t components, glyphs, lines, rejected;
    uint8_t minimum_confidence; /* Classifier probability percent; not a guarantee. */
} OCRDiagnostics;

/* Standalone block-handwriting recognizer, trained on EMNIST. Recognizes
 * separated A-Z/0-9, spaces, line breaks and colons; output is uppercase.
 * Not cursive, arbitrary punctuation, or a substitute for optical focus.
 * No heap allocation, network, PC, test files or font-template dependency.
 * One control loop; static scratch storage is reused each call. */
AppStatus ocr_init(void);
/* Initialize camera and capture separately; recognizes the current frame.
 * APP_NOT_READY before init, without a frame, for empty/ambiguous input.
 * APP_ERROR for bad arguments/frame/ROI or insufficient output capacity.
 * On failure a valid output buffer is cleared. Never returns canned text. */
AppStatus ocr_read_text(char *buffer, uint16_t capacity);
/* Same recognizer on a supplied validated RGB565 frame; also works on host. */
AppStatus ocr_read_frame(const CameraFrame *frame, char *buffer, uint16_t capacity);
AppStatus ocr_configure(const OCRConfig *config);
AppStatus ocr_get_diagnostics(OCRDiagnostics *diagnostics);
#ifdef __cplusplus
}
#endif
#endif
