
#include "UV_LED.h"
#include "AppConfig.h"

#ifdef PICO_ON_DEVICE
#include "hardware/gpio.h"
#endif

AppStatus uv_led_init(void)
{
#ifdef PICO_ON_DEVICE
    gpio_init(PIN_UV_LED);
    gpio_put(PIN_UV_LED, 0);
    gpio_set_dir(PIN_UV_LED, GPIO_OUT);

    return APP_OK;
#else
    return APP_UNSUPPORTED;
#endif
}

AppStatus uv_led_set(bool enabled)
{
#ifdef PICO_ON_DEVICE
    gpio_put(PIN_UV_LED, enabled);

    return APP_OK;
#else
    (void)enabled;
    return APP_UNSUPPORTED;
#endif
}
