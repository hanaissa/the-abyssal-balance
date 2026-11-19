
#include "WaterLevel.h"
#include "AppConfig.h"
#include <stddef.h>

#ifdef PICO_ON_DEVICE
#include "hardware/adc.h"
#endif

AppStatus water_level_init(void)
{
#ifdef PICO_ON_DEVICE
    adc_init();
    adc_gpio_init(PIN_WATER_LEVEL);

    return APP_OK;
#else
    return APP_UNSUPPORTED;
#endif
}

AppStatus water_level_read(uint16_t *level)
{
    if (level == NULL)
    {
        return APP_ERROR;
    }

#ifdef PICO_ON_DEVICE
    adc_select_input(1);
    *level = adc_read();

    return APP_OK;
#else
    return APP_UNSUPPORTED;
#endif
}

bool water_level_within_tolerance(
    uint16_t level,
    uint16_t target,
    uint16_t tolerance)
{
    int difference = (int)level - (int)target;

    return difference >= -(int)tolerance &&
           difference <= (int)tolerance;
}
