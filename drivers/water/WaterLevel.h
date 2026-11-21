#ifndef ABYSSAL_WATER_LEVEL_H
#define ABYSSAL_WATER_LEVEL_H
#include "AppTypes.h"
/* Initialize ADC and apply defaults: dry=1..90, 50%=1850, 75%=2000,
 * full=2100 ADC counts. Values between anchors interpolate. */
AppStatus water_level_init(void);
typedef struct {
    uint16_t minimum, center, maximum;
} WaterLevelCalibrationPoint;
/* Apply dry/50%/75%/full ranges in order. Each must be 0..4095 with its
 * center inside the range; neighboring ranges must not overlap or touch.
 * Invalid input disables calibration. No overlap rule or saved profile is
 * applied on invalid input. Custom calibration is RAM-only; init restores
 * the default anchors. */
AppStatus water_level_set_calibration(const WaterLevelCalibrationPoint points[4]);
void water_level_clear_calibration(void);
bool water_level_is_calibrated(void);
/* Capture 16 median-filtered measurements (~1.5 seconds) at one fixed depth.
 * Returns their observed range and median; does not apply calibration. */
AppStatus water_level_capture_calibration_point(WaterLevelCalibrationPoint *point);
/* Filtered, calibrated level: dry=0, half=500, 75%=750, full=1000.
 * APP_NOT_READY if calibration has been cleared/rejected. Takes five
 * ADC samples 10 ms apart. Clears output on failure. Raw sample remains
 * available after an uncalibrated read so applications can diagnose/calibrate. */
AppStatus water_level_read(uint16_t *level);
/* Unscaled, unfiltered ADC sample (0..4095), for wiring/calibration diagnostics. */
AppStatus water_level_read_raw(uint16_t *raw);
/* Median ADC sample used by the last successful water_level_read().
 * APP_NOT_READY after init or a sampling failure; clears output on failure. */
AppStatus water_level_get_last_raw(uint16_t *raw);
bool water_level_within_tolerance(uint16_t level, uint16_t target, uint16_t tolerance);
#endif
