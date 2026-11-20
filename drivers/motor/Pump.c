#include "Pump.h"
#include "L298N.h"

#define PUMP_MAX_SPEED_PERCENT 100u

AppStatus pump_init(void) {
    AppStatus status;

    l298n_emergency_stop();
    status = l298n_init();
    if (status != APP_OK) {
        l298n_emergency_stop();
    }
    return status;
}

AppStatus pump_apply(PumpCommand command) {
    PumpDirection direction;
    uint8_t speed_percent;
    AppStatus status;

    switch (command.direction) {
        case PUMP_STOP:
            direction = PUMP_STOP;
            speed_percent = 0u;
            break;
        case PUMP_FILL:
        case PUMP_DRAIN:
            if (command.speed_percent > PUMP_MAX_SPEED_PERCENT) {
                l298n_emergency_stop();
                return APP_ERROR;
            }
            if (command.speed_percent == 0u) {
                direction = PUMP_STOP;
                speed_percent = 0u;
            } else {
                direction = command.direction;
                speed_percent = command.speed_percent;
            }
            break;
        default:
            l298n_emergency_stop();
            return APP_ERROR;
    }

    status = l298n_apply(direction, speed_percent);
    if (status != APP_OK) {
        l298n_emergency_stop();
    }
    return status;
}

AppStatus pump_start_fill(uint8_t speed_percent) {
    return pump_apply((PumpCommand){ PUMP_FILL, speed_percent });
}

AppStatus pump_start_drain(uint8_t speed_percent) {
    return pump_apply((PumpCommand){ PUMP_DRAIN, speed_percent });
}

void pump_stop(void) {
    l298n_emergency_stop();
}
