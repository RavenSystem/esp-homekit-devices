/* Simple example for I2C / BMP180 / Timer & Event Handling
 *
 * This sample code is in the public domain.
 */
#include "espressif/esp_common.h"
#include "esp/uart.h"

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"

// BMP180 driver
#include "bmp180/bmp180.h"

#define I2C_BUS 0
#define SCL_PIN GPIO_ID_PIN((0))
#define SDA_PIN GPIO_ID_PIN((2))

#define MY_EVT_TIMER  0x01
#define MY_EVT_BMP180 0x02

typedef struct
{
    uint8_t event_type;
    bmp180_result_t bmp180_data;
} my_event_t;

//device descriptor
i2c_dev_t dev = {
    .addr = BMP180_DEVICE_ADDRESS,
    .bus = I2C_BUS,
};

// Communication Queue
static QueueHandle_t mainqueue;
static TimerHandle_t timerHandle;

// Own BMP180 User Inform Implementation
bool bmp180_i2c_informUser(const QueueHandle_t* resultQueue, uint8_t cmd, bmp180_temp_t temperature, bmp180_press_t pressure)
{
    my_event_t ev;

    ev.event_type = MY_EVT_BMP180;
    ev.bmp180_data.cmd = cmd;
    ev.bmp180_data.temperature = temperature;
    ev.bmp180_data.pressure = pressure;

    return (xQueueSend(*resultQueue, &ev, 0) == pdTRUE);
}

// Timer call back
static void bmp180_i2c_timer_cb(TimerHandle_t xTimer)
{
    my_event_t ev;
    ev.event_type = MY_EVT_TIMER;

    xQueueSend(mainqueue, &ev, 0);
}

// Check for communiction events
void bmp180_task(void *pvParameters)
{
    // Received pvParameters is communication queue
    QueueHandle_t *com_queue = (QueueHandle_t *)pvParameters;

    printf("%s: Started user interface task\n", __FUNCTION__);

    while(1)
    {
        my_event_t ev;

        xQueueReceive(*com_queue, &ev, portMAX_DELAY);

        switch(ev.event_type)
        {
        case MY_EVT_TIMER:
            printf("%s: Received Timer Event\n", __FUNCTION__);

            bmp180_trigger_measurement(&dev, com_queue);
            break;
        case MY_EVT_BMP180:
            printf("%s: Received BMP180 Event temp:=%d.%dC press=%d.%02dhPa\n", __FUNCTION__, \
                       (int32_t)ev.bmp180_data.temperature, abs((int32_t)(ev.bmp180_data.temperature*10)%10), \
                       ev.bmp180_data.pressure/100, ev.bmp180_data.pressure%100 );
            break;
        default:
            break;
        }
    }
}

// Setup HW
void user_setup(void)
{
    // Set UART Parameter
    uart_set_baud(0, 115200);

    // Give the UART some time to settle
    sdk_os_delay_us(500);

    // Init I2C bus Interface
    i2c_init(I2C_BUS, SCL_PIN, SDA_PIN, I2C_FREQ_100K);
}

void user_init(void)
{
    // Setup HW
    user_setup();

    // Just some infomations
    printf("\n");
    printf("SDK version : %s\n", sdk_system_get_sdk_version());
    printf("GIT version : %s\n", GITSHORTREV);

    // Use our user inform implementation
    bmp180_informUser = bmp180_i2c_informUser;

    // Init BMP180 Interface
    bmp180_init(&dev);

    // Create Main Communication Queue
    mainqueue = xQueueCreate(10, sizeof(my_event_t));

    // Create user interface task
    xTaskCreate(bmp180_task, "bmp180_task", 256, &mainqueue, 2, NULL);

    // Create Timer (Trigger a measurement every second)
    timerHandle = xTimerCreate("BMP180 Trigger", 1000/portTICK_PERIOD_MS, pdTRUE, NULL, bmp180_i2c_timer_cb);

    if (timerHandle != NULL)
    {
        if (xTimerStart(timerHandle, 0) != pdPASS)
        {
            printf("%s: Unable to start Timer ...\n", __FUNCTION__);
        }
    }
    else
    {
        printf("%s: Unable to create Timer ...\n", __FUNCTION__);
    }
}
