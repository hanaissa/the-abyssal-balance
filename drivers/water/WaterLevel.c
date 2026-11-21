
#include "WaterLevel.h"
#include "AppConfig.h"
#include <stddef.h>

#ifdef PICO_ON_DEVICE
#include "hardware/adc.h"
#include "pico/stdlib.h"
#endif

static uint16_t last_raw;
static bool last_raw_valid;
static bool initialized;
static WaterLevelCalibrationPoint calibration[4];
static bool calibrated;

/* Owner-selected ADC anchors. Retain the last measured dry band. */
static const WaterLevelCalibrationPoint default_calibration[4] = {
    {1, 41, 90},
    {1850, 1850, 1850},
    {2000, 2000, 2000},
    {2100, 2100, 2100}
};

void water_level_clear_calibration(void) {
    calibrated = false;
}

bool water_level_is_calibrated(void) {
    return calibrated;
}

AppStatus water_level_set_calibration(const WaterLevelCalibrationPoint points[4]) {
    water_level_clear_calibration();
    if (!points) { return APP_ERROR; }
    for (unsigned i = 0; i < 4; ++i) {
        if (points[i].minimum > points[i].center ||
            points[i].center > points[i].maximum || points[i].maximum > 4095u ||
            (i && points[i - 1].maximum >= points[i].minimum)) {
            return APP_ERROR;
        }
    }
    for (unsigned i = 0; i < 4; ++i) { calibration[i] = points[i]; }
    calibrated = true;
    return APP_OK;
}

/* Validated bands are separate; interpolate only in the gaps. */
static uint16_t scale_raw(uint16_t raw) {
    static const uint16_t level[] = {0, 500, 750, 1000};
    for (unsigned i = 0; i < 4; ++i) {
        if (raw <= calibration[i].maximum) {
            if (!i || raw >= calibration[i].minimum) { return level[i]; }
            uint32_t span = calibration[i].minimum - calibration[i - 1].maximum;
            uint32_t value = (uint32_t)(raw - calibration[i - 1].maximum) *
                             (level[i] - level[i - 1]);
            return (uint16_t)(level[i - 1] + (value + span / 2) / span);
        }
    }
    return 1000;
}

AppStatus water_level_init(void)
{
    initialized = false;
    last_raw_valid = false;
    water_level_clear_calibration();
#ifdef PICO_ON_DEVICE
    adc_init();
    adc_gpio_init(PIN_WATER_LEVEL);
    initialized = true;

    return water_level_set_calibration(default_calibration);
#else
    return APP_UNSUPPORTED;
#endif
}

AppStatus water_level_read_raw(uint16_t *level)
{
    if (level == NULL)
    {
        return APP_ERROR;
    }
    *level = 0;

#ifdef PICO_ON_DEVICE
    if (!initialized) { return APP_NOT_READY; }
    adc_select_input(1);
    *level = adc_read();
    if (*level > 4095u) { *level = 0; return APP_ERROR; }

    return APP_OK;
#else
    return APP_UNSUPPORTED;
#endif
}

static AppStatus read_filtered_raw(uint16_t *raw) {
    last_raw_valid = false;
    *raw = 0;
    uint16_t samples[5];
    for (unsigned i = 0; i < 5; ++i) {
        AppStatus status = water_level_read_raw(&samples[i]);
        if (status != APP_OK) { return status; }
        unsigned j = i;
        while (j > 0 && samples[j] < samples[j - 1]) {
            uint16_t previous = samples[j - 1];
            samples[j - 1] = samples[j];
            samples[j] = previous;
            --j;
        }
#ifdef PICO_ON_DEVICE
        if (i + 1 < 5) { sleep_ms(10); }
#endif
    }
    last_raw = samples[2];
    last_raw_valid = true;
    *raw = last_raw;
    return APP_OK;
}

AppStatus water_level_read(uint16_t *level) {
    if (!level) { last_raw_valid = false; return APP_ERROR; }
    *level = 0;
    uint16_t raw;
    AppStatus status = read_filtered_raw(&raw);
    if (status != APP_OK) { return status; }
    if (!calibrated) { return APP_NOT_READY; }
    *level = scale_raw(raw);
    return APP_OK;
}

AppStatus water_level_capture_calibration_point(WaterLevelCalibrationPoint *point) {
    if (!point) { return APP_ERROR; }
    *point = (WaterLevelCalibrationPoint){0};
    uint16_t values[16];
    for (unsigned i = 0; i < 16; ++i) {
        AppStatus status = read_filtered_raw(&values[i]);
        if (status != APP_OK) { return status; }
        unsigned j = i;
        while (j && values[j] < values[j - 1]) {
            uint16_t previous = values[j - 1];
            values[j - 1] = values[j];
            values[j] = previous;
            --j;
        }
#ifdef PICO_ON_DEVICE
        if (i + 1 < 16) { sleep_ms(60); }
#endif
    }
    point->minimum = values[0];
    point->center = (uint16_t)(((uint32_t)values[7] + values[8]) / 2);
    point->maximum = values[15];
    return APP_OK;
}

AppStatus water_level_get_last_raw(uint16_t *raw) {
    if (!raw) { return APP_ERROR; }
    *raw = 0;
    if (!last_raw_valid) { return APP_NOT_READY; }
    *raw = last_raw;
    return APP_OK;
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
