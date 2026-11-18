#ifndef ABYSSAL_APP_TYPES_H
#define ABYSSAL_APP_TYPES_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { APP_OK = 0, APP_ERROR, APP_NOT_READY, APP_UNSUPPORTED } AppStatus;
typedef enum { PUMP_STOP, PUMP_FILL, PUMP_DRAIN } PumpDirection;
typedef struct { PumpDirection direction; uint8_t speed_percent; } PumpCommand;
typedef enum { STATE_WAITING_FOR_RFID, STATE_BALANCING, STATE_CHECKING, STATE_FAILURE_RECOVERY, STATE_REWARD_SEQUENCE, STATE_RESETTING } SystemState;
typedef struct { uint16_t uid[10]; uint8_t length; } RfidUid;
typedef struct { bool submit_pressed; bool reset_pressed; } ButtonEvents;
#endif
