#include "Potentiometer.h"
#include "AppConfig.h"
#include <stddef.h>

#ifdef PICO_ON_DEVICE
#include "hardware/adc.h"
#endif

AppStatus potentiometer_init(void)
{
#ifdef PICO_ON_DEVICE

    adc_init();
    adc_gpio_init(PIN_POTENTIOMETER);

    return APP_OK;

#else
    return APP_UNSUPPORTED;
#endif
}

AppStatus potentiometer_read_percent(uint8_t *percent)
{
    if (percent == NULL)
    {
        return APP_ERROR;
    }

#ifdef PICO_ON_DEVICE

    adc_select_input(0);

    uint16_t raw = adc_read();

    *percent = (raw * 100u) / 4095u;

    return APP_OK;

#else
    return APP_UNSUPPORTED;
#endif
}

AppStatus potentiometer_command(PumpCommand *command)
{
    if (command == NULL)
    {
        return APP_ERROR;
    }

    uint8_t percent;

    AppStatus status = potentiometer_read_percent(&percent);

    if (status != APP_OK)
    {
        return status;
    }

    int lower = 50 - POTENTIOMETER_DEAD_ZONE_PERCENT;
    int upper = 50 + POTENTIOMETER_DEAD_ZONE_PERCENT;

    if (percent < lower)
    {
        command->direction = PUMP_DRAIN;
        command->speed_percent =
            (lower - percent) * 100 / lower;
    }
    else if (percent > upper)
    {
        command->direction = PUMP_FILL;
        command->speed_percent =
            (percent - upper) * 100 / (100 - upper);
    }
    else
    {
        command->direction = PUMP_STOP;
        command->speed_percent = 0;
    }

    return APP_OK;
}