/* Test code for DS3231 high precision RTC module
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Bhuvanchandra DV <bhuvanchandra.dv@gmail.com>
 * MIT Licensed as described in the file LICENSE
 */
#include "espressif/esp_common.h"
#include "esp/uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "i2c/i2c.h"

#include "ds3231/ds3231.h"

#define ADDR DS3231_ADDR
#define I2C_BUS 0

void task1(void *pvParameters)
{
    struct tm time;
    float tempFloat;
    i2c_dev_t dev = {
        .addr = ADDR,
        .bus = I2C_BUS,
    };
    while(1) {
        vTaskDelay(100);
        ds3231_getTime(&dev, &time);
	    ds3231_getTempFloat(&dev, &tempFloat);
        printf("TIME:%d:%d:%d, TEMPERATURE:%.2f DegC\r\n", time.tm_hour, time.tm_min, time.tm_sec, tempFloat);
    }
}

void user_init(void)
{
    const int scl = 5, sda = 4;

    uart_set_baud(0, 115200);

    printf("\n");
    printf("SDK version : %s\n", sdk_system_get_sdk_version());
    printf("GIT version : %s\n", GITSHORTREV);

    i2c_init(0, scl, sda, I2C_FREQ_400K);

    xTaskCreate(task1, "tsk1", 256, NULL, 2, NULL);
}
