/*
 * Driver for ultrasonic range meters, e.g. HC-SR04, HY-SRF05 and so on
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#include "ultrasonic.h"
#include <esp/gpio.h>
#include <espressif/esp_common.h>
#include <stdio.h>

#define TRIGGER_LOW_DELAY 4
#define TRIGGER_HIGH_DELAY 10
#define PING_TIMEOUT 6000
#define ROUNDTRIP 58

#define timeout_expired(start, len) ((uint32_t)(sdk_system_get_time() - (start)) >= (len))

void ultrasoinc_init(const ultrasonic_sensor_t *dev)
{
    gpio_enable(dev->trigger_pin, GPIO_OUTPUT);
    gpio_enable(dev->echo_pin, GPIO_INPUT);
    gpio_write(dev->trigger_pin, false);
}

int32_t ultrasoinc_measure_cm(const ultrasonic_sensor_t *dev, uint32_t max_distance)
{
    // Ping: Low for 2..4 us, then high 10 us
    gpio_write(dev->trigger_pin, false);
    sdk_os_delay_us(TRIGGER_LOW_DELAY);
    gpio_write(dev->trigger_pin, true);
    sdk_os_delay_us(TRIGGER_HIGH_DELAY);
    gpio_write(dev->trigger_pin, false);

    // Previous ping isn't ended
    if (gpio_read(dev->echo_pin))
        return ULTRASONIC_ERROR_PING;

    // Wait for echo
    uint32_t start = sdk_system_get_time();
    while (!gpio_read(dev->echo_pin))
    {
        if (timeout_expired(start, PING_TIMEOUT))
            return ULTRASONIC_ERROR_PING_TIMEOUT;
    }

    // got echo, measuring
    uint32_t echo_start = sdk_system_get_time();
    uint32_t time = echo_start;
    uint32_t meas_timeout = echo_start + max_distance * ROUNDTRIP;
    while (gpio_read(dev->echo_pin))
    {
        time = sdk_system_get_time();
        if (timeout_expired(echo_start, meas_timeout))
            return ULTRASONIC_ERROR_ECHO_TIMEOUT;
    }

    return (time - echo_start) / ROUNDTRIP;
}
