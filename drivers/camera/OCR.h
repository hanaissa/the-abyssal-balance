#ifndef ABYSSAL_OCR_H
#define ABYSSAL_OCR_H
#include "AppTypes.h"
AppStatus ocr_init(void);
AppStatus ocr_read_text(char *buffer, uint16_t capacity);
#endif
