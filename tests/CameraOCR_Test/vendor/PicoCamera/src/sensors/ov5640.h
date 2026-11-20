/*
 * This file is part of the PicoCamera project.
 * https://github.com/umeiko/PicoCamera
 *
 * Author: umeko <umeko@stu.xmu.edu.cn>
 * License: MIT
 */

/**
 * @file ov5640.h
 * @brief OV5640 sensor driver (16-bit SCCB register addresses).
 */
#pragma once

#include "../sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OV5640_SCCB_ADDR    0x3C    // 0x78 >> 1
#define OV5640_PID          0x5640

/**
 * @brief Probe the bus for an OV5640. On success fills sensor->id.
 * @return 0 if detected
 */
int ov5640_detect(sensor_t *sensor);

/**
 * @brief Fill sensor_t function pointers and write the default register set.
 * @return 0 on success
 */
int ov5640_init_sensor(sensor_t *sensor);

#ifdef __cplusplus
}
#endif
