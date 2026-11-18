#ifndef ABYSSAL_APP_EVENTS_H
#define ABYSSAL_APP_EVENTS_H
#include "AppTypes.h"
typedef enum { APP_EVENT_NONE, APP_EVENT_RFID_AUTHORIZED, APP_EVENT_SUBMIT, APP_EVENT_RESET, APP_EVENT_OCR_READY, APP_EVENT_ERROR } AppEventType;
typedef struct { AppEventType type; uint16_t target_level; const char *text; } AppEvent;
#endif
