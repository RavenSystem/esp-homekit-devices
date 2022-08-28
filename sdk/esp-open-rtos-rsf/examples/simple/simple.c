/* Very basic example that just demonstrates we can run at all!
 */
#include "espressif/esp_common.h"
#include "esp/uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

void task1(void *pvParameters)
{
    QueueHandle_t *queue = (QueueHandle_t *)pvParameters;
    printf("Hello from task1!\r\n");
    uint32_t count = 0;
    while(1) {
        vTaskDelay(100);
        xQueueSend(*queue, &count, 0);
        count++;
    }
}

void task2(void *pvParameters)
{
    printf("Hello from task 2!\r\n");
    QueueHandle_t *queue = (QueueHandle_t *)pvParameters;
    while(1) {
        uint32_t count;
        if(xQueueReceive(*queue, &count, 1000)) {
            printf("Got %u\n", count);
        } else {
            printf("No msg :(\n");
        }
    }
}

static QueueHandle_t mainqueue;

void user_init(void)
{
    uart_set_baud(0, 115200);
    printf("SDK version:%s\n", sdk_system_get_sdk_version());
    mainqueue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(task1, "tsk1", 256, &mainqueue, 2, NULL);
    xTaskCreate(task2, "tsk2", 256, &mainqueue, 2, NULL);
}
