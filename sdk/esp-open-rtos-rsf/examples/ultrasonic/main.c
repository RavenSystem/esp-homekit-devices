/*
 * Example of using ultrasonic rnaghe meter like HC-SR04
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <esp/gpio.h>
#include <ultrasonic/ultrasonic.h>

#define TRIGGER_PIN 5
#define ECHO_PIN    4

#define MAX_DISTANCE_CM 500 // 5m max

void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i ++)
        sdk_os_delay_us(1000);
}

void user_init()
{
    uart_set_baud(0, 115200);
    printf("SDK version : %s\n", sdk_system_get_sdk_version());

    ultrasonic_sensor_t sensor = {
        .trigger_pin = TRIGGER_PIN,
        .echo_pin = ECHO_PIN
    };

    ultrasoinc_init(&sensor);

    while (true)
    {
        int32_t distance = ultrasoinc_measure_cm(&sensor, MAX_DISTANCE_CM);
        if (distance < 0)
        {
            printf("Error: ");
            switch (distance)
            {
                case ULTRASONIC_ERROR_PING:
                    printf("Cannot ping (device is in invalid state)\n");
                    break;
                case ULTRASONIC_ERROR_PING_TIMEOUT:
                    printf("Ping timeout (no device found)\n");
                    break;
                case ULTRASONIC_ERROR_ECHO_TIMEOUT:
                    printf("Echo timeout (i.e. distance too big)\n");
                    break;
            }
        }
        else
            printf("Distance: %d cm, %.02f m\n", distance, distance / 100.0);

        delay_ms(200);
    }
}
