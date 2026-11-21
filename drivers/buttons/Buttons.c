#include "Buttons.h"
#include "../../include/AppConfig.h"
#include <stddef.h>

#ifdef PICO_ON_DEVICE
#include "hardware/gpio.h"
#include "pico/stdlib.h"
static ButtonEvents previous;
static bool ready;
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

    previous = (ButtonEvents){0};
    ready = true;

    return APP_OK;

#else
    return APP_UNSUPPORTED;
#endif
}

AppStatus buttons_poll_debounced(ButtonEvents *events) {
    if (!events) { return APP_ERROR; }
    *events = (ButtonEvents){0};
#ifdef PICO_ON_DEVICE
    if (!ready) { return APP_NOT_READY; }
    ButtonEvents first, second;
    AppStatus status = buttons_poll(&first);
    if (status != APP_OK) { return status; }
    sleep_ms(20);
    status = buttons_poll(&second);
    if (status != APP_OK) { return status; }
    if (first.submit_pressed == second.submit_pressed) {
        events->submit_pressed = second.submit_pressed && !previous.submit_pressed;
        previous.submit_pressed = second.submit_pressed;
    }
    if (first.reset_pressed == second.reset_pressed) {
        events->reset_pressed = second.reset_pressed && !previous.reset_pressed;
        previous.reset_pressed = second.reset_pressed;
    }
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
