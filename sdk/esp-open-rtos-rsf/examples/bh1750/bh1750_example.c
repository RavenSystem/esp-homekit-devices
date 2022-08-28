#include <stdio.h>

#include "espressif/esp_common.h"
#include "esp/uart.h"

#include "FreeRTOS.h"
#include "task.h"

#include "i2c/i2c.h"
#include "bh1750/bh1750.h"

#define SCL_PIN 5
#define SDA_PIN 4
#define I2C_BUS 0

static void measure(void *pvParameters)
{
    i2c_dev_t dev = {
        .addr = BH1750_ADDR_LO,
        .bus = I2C_BUS,
    };
    bh1750_configure(&dev, BH1750_CONTINUOUS_MODE | BH1750_HIGH_RES_MODE);

    while (1) {
        while(1) {
            vTaskDelay(200 / portTICK_PERIOD_MS);
            printf("Lux: %d\n", bh1750_read(&dev));
        }
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
