/*
 * Driver for ultrasonic range meters, e.g. HC-SR04, HY-SRF05 and so on
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#ifndef EXTRAS_ULTRASONIC_H_
#define EXTRAS_ULTRASONIC_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ULTRASONIC_ERROR_PING         (-1)
#define ULTRASONIC_ERROR_PING_TIMEOUT (-2)
#define ULTRASONIC_ERROR_ECHO_TIMEOUT (-3)

/**
 * Device descriptor
 */
typedef struct
{
    uint8_t trigger_pin;
    uint8_t echo_pin;
} ultrasonic_sensor_t;

/**
 * Init ranging module
 * \param dev Pointer to the device descriptor
 */
void ultrasoinc_init(const ultrasonic_sensor_t *dev);

/**
 * Measure distance
 * \param dev Pointer to the device descriptor
 * \param max_distance Maximal distance to measure, centimeters
 * \return Distance in centimeters or ULTRASONIC_ERROR_xxx if error occured
 */
int32_t ultrasoinc_measure_cm(const ultrasonic_sensor_t *dev, uint32_t max_distance);

#ifdef __cplusplus
}
#endif

#endif /* EXTRAS_ULTRASONIC_H_ */
