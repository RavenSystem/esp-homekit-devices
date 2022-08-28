#include <stdio.h>

#include "espressif/esp_common.h"
#include "esp/uart.h"

#include "FreeRTOS.h"
#include "task.h"

#include "i2c/i2c.h"
#include "pcf8591/pcf8591.h"

#define ADDR PCF8591_DEFAULT_ADDRESS
#define I2C_BUS 0
#define SCL_PIN 5
#define SDA_PIN 4

static void measure(void *pvParameters)
{
    i2c_dev_t dev = {
        .addr = ADDR,
        .bus = I2C_BUS,
    };
    while (1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        printf("Value: %d\n", pcf8591_read(&dev, PCF8591_IC_4_SINGLES, 3));
    }
}

void user_init(void)
{
    uart_set_baud(0, 115200);

    // Just some information
    printf("\n");
    printf("SDK version : %s\n", sdk_system_get_sdk_version());
    printf("GIT version : %s\n", GITSHORTREV);

    i2c_init(I2C_BUS, SCL_PIN, SDA_PIN, I2C_FREQ_100K);

    xTaskCreate(measure, "measure_task", 256, NULL, 2, NULL);
}
