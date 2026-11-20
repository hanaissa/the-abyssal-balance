#include "L298N.h"
#include "AppConfig.h"

#ifdef PICO_ON_DEVICE
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"

#define L298N_PWM_FREQ_HZ 10000u
#define L298N_PWM_COUNTS 1000u
#define L298N_PWM_TOP (L298N_PWM_COUNTS - 1u)
#define L298N_MAX_SPEED_PERCENT 100u
#define L298N_DEADTIME_US 10u

/* GP8/GP9 control only ENA/ENB. IN1/IN3 are hardwired HIGH and IN2/IN4
 * LOW. The 12 V pumps use external VS power, not Pico 3V3 OUT.
 * Duty is enable on-time, not a measured motor voltage or flow rate. */

static bool g_ready = false;
static unsigned int g_slice = 0u;
static PumpDirection g_active_direction = PUMP_STOP;

static void l298n_force_all_off(void) {
    gpio_set_outover(PIN_PUMP_FILL_ENABLE, GPIO_OVERRIDE_LOW);
    gpio_set_outover(PIN_PUMP_DRAIN_ENABLE, GPIO_OVERRIDE_LOW);
    pwm_set_enabled(g_slice, false);
    pwm_set_both_levels(g_slice, 0u, 0u);
    pwm_set_counter(g_slice, 0u);
    g_active_direction = PUMP_STOP;
}

AppStatus l298n_init(void) {
    if (g_ready) {
        l298n_force_all_off();
        return APP_OK;
    }

    const unsigned int fill_slice = pwm_gpio_to_slice_num(PIN_PUMP_FILL_ENABLE);
    const unsigned int drain_slice = pwm_gpio_to_slice_num(PIN_PUMP_DRAIN_ENABLE);
    const unsigned int fill_chan = pwm_gpio_to_channel(PIN_PUMP_FILL_ENABLE);
    const unsigned int drain_chan = pwm_gpio_to_channel(PIN_PUMP_DRAIN_ENABLE);
    const uint32_t sys_hz = clock_get_hz(clk_sys);
    const float clkdiv =
        (float)sys_hz / (float)(L298N_PWM_FREQ_HZ * L298N_PWM_COUNTS);
    pwm_config cfg;

    if (PIN_PUMP_FILL_ENABLE == PIN_PUMP_DRAIN_ENABLE ||
        fill_slice != drain_slice ||
        fill_chan != PWM_CHAN_A ||
        drain_chan != PWM_CHAN_B ||
        clkdiv < 1.0f ||
        clkdiv >= 256.0f) {
        g_ready = false;
        g_active_direction = PUMP_STOP;
        return APP_ERROR;
    }

    gpio_set_outover(PIN_PUMP_FILL_ENABLE, GPIO_OVERRIDE_LOW);
    gpio_set_outover(PIN_PUMP_DRAIN_ENABLE, GPIO_OVERRIDE_LOW);

    pwm_set_enabled(fill_slice, false);
    cfg = pwm_get_default_config();
    pwm_config_set_wrap(&cfg, L298N_PWM_TOP);
    pwm_config_set_clkdiv(&cfg, clkdiv);
    pwm_init(fill_slice, &cfg, false);
    pwm_set_both_levels(fill_slice, 0u, 0u);
    pwm_set_counter(fill_slice, 0u);

    gpio_set_function(PIN_PUMP_FILL_ENABLE, GPIO_FUNC_PWM);
    gpio_set_function(PIN_PUMP_DRAIN_ENABLE, GPIO_FUNC_PWM);

    g_slice = fill_slice;
    // Selecting a GPIO function clears its overrides in the Pico SDK.
    l298n_force_all_off();
    g_ready = true;
    return APP_OK;
}

AppStatus l298n_apply(PumpDirection direction, uint8_t speed_percent) {
    uint16_t level;
    uint16_t level_a;
    uint16_t level_b;
    unsigned int active_pin;

    if (!g_ready) {
        return APP_NOT_READY;
    }

    switch (direction) {
        case PUMP_STOP:
            l298n_force_all_off();
            return APP_OK;
        case PUMP_FILL:
        case PUMP_DRAIN:
            if (speed_percent > L298N_MAX_SPEED_PERCENT) {
                l298n_force_all_off();
                return APP_ERROR;
            }
            if (speed_percent == 0u) {
                l298n_force_all_off();
                return APP_OK;
            }
            break;
        default:
            l298n_force_all_off();
            return APP_ERROR;
    }

    level = (uint16_t)(((uint32_t)speed_percent * L298N_PWM_COUNTS) /
                       L298N_MAX_SPEED_PERCENT);
    level_a = (direction == PUMP_FILL) ? level : 0u;
    level_b = (direction == PUMP_DRAIN) ? level : 0u;

    if (g_active_direction == direction) {
        pwm_set_both_levels(g_slice, level_a, level_b);
        return APP_OK;
    }

    l298n_force_all_off();
    pwm_set_both_levels(g_slice, level_a, level_b);
    busy_wait_us_32(L298N_DEADTIME_US);
    pwm_set_enabled(g_slice, true);
    active_pin = (direction == PUMP_FILL) ? PIN_PUMP_FILL_ENABLE
                                          : PIN_PUMP_DRAIN_ENABLE;
    gpio_set_outover(active_pin, GPIO_OVERRIDE_NORMAL);
    g_active_direction = direction;
    return APP_OK;
}

void l298n_emergency_stop(void) {
    if (!g_ready) {
        return;
    }
    l298n_force_all_off();
}
#else
AppStatus l298n_init(void) {
    return APP_NOT_READY;
}

AppStatus l298n_apply(PumpDirection direction, uint8_t speed_percent) {
    (void)direction;
    (void)speed_percent;
    return APP_NOT_READY;
}

void l298n_emergency_stop(void) {}
#endif
