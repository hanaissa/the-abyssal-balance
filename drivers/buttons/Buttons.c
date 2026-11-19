#include "Buttons.h"
#include "../../include/AppConfig.h"
#include <stddef.h>

#ifdef PICO_ON_DEVICE
#include "hardware/gpio.h"
#endif

AppStatus buttons_init(void)
{
#ifdef PICO_ON_DEVICE

    gpio_init(PIN_SUBMIT_BUTTON);
    gpio_set_dir(PIN_SUBMIT_BUTTON, GPIO_IN);
    gpio_pull_up(PIN_SUBMIT_BUTTON);

    gpio_init(PIN_RESET_BUTTON);
    gpio_set_dir(PIN_RESET_BUTTON, GPIO_IN);
    gpio_pull_up(PIN_RESET_BUTTON);

    return APP_OK;

#else
    return APP_UNSUPPORTED;
#endif
}

AppStatus buttons_poll(ButtonEvents *events)
{
    if (events == NULL)
    {
        return APP_ERROR;
    }

#ifdef PICO_ON_DEVICE

    events->submit_pressed = !gpio_get(PIN_SUBMIT_BUTTON);
    events->reset_pressed = !gpio_get(PIN_RESET_BUTTON);

    return APP_OK;

#else
    return APP_UNSUPPORTED;
#endif
}