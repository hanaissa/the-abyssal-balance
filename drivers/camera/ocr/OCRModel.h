#ifndef ABYSSAL_OCR_MODEL_H
#define ABYSSAL_OCR_MODEL_H
#define OCR_SIDE 24
#define OCR_EXTENT 20
#define OCR_INPUT (OCR_SIDE * OCR_SIDE)
#define OCR_HIDDEN 192
#define OCR_CLASSES 36
extern const float ocr_w1[OCR_INPUT * OCR_HIDDEN];
extern const float ocr_b1[OCR_HIDDEN];
extern const float ocr_w2[OCR_HIDDEN * OCR_CLASSES];
extern const float ocr_b2[OCR_CLASSES];
#endif
