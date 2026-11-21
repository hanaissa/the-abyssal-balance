#ifndef ABYSSAL_OCR_INTERNAL_H
#define ABYSSAL_OCR_INTERNAL_H
#include <stdint.h>
#include "OCRModel.h"
/* Model parity/testing entry point: normalized foreground coverage. */
char ocr_classify_glyph(const uint8_t pixels[OCR_INPUT], uint8_t *confidence);
#endif
