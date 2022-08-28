/**
 * Simple example with one sensor connected to I2C bus 0. It demonstrates the
 * different approaches to fetch the data. Either the interrupt *nINT* is used
 * whenever new data are available or exceed defined thresholds or the new
 * data are fetched periodically.
 *
 * Harware configuration:
 *
 *   +-----------------+   +----------+
 *   | ESP8266 / ESP32 |   | CCS811   |
 *   |                 |   |          |
 *   |   GPIO 14 (SCL) ----> SCL      |
 *   |   GPIO 13 (SDA) <---> SDA      |
 *   |   GPIO 5        <---- INT1     |
 *   |   GND           ----> /WAKE    |
 *   +-----------------+   +----------+
 */

/* -- use following constants to define the example mode ----------- */

// #define INT_DATA_RDY_USED
// #define INT_THRESHOLD_USED

#if defined(INT_DATA_RDY_USED) || defined(INT_THRESHOLD_USED)
#define INT_USED
#endif

/* -- includes ----------------------------------------------------- */

#include "ccs811.h"

/* -- platform dependent definitions ------------------------------- */

#ifdef ESP_PLATFORM  // ESP32 (ESP-IDF)

// user task stack depth for ESP32
#define TASK_STACK_DEPTH 2048

#else  // ESP8266 (esp-open-rtos)

// user task stack depth for ESP8266
#define TASK_STACK_DEPTH 256

#endif  // ESP_PLATFORM

// I2C interface defintions for ESP32 and ESP8266
#define I2C_BUS       0
#define I2C_SCL_PIN   14
#define I2C_SDA_PIN   13
#define I2C_FREQ      I2C_FREQ_100K

// interrupt GPIOs defintions for ESP8266 and ESP32
#define nINT_PIN      13

/* -- user tasks --------------------------------------------------- */

static ccs811_sensor_t* sensor;

#ifdef INT_USED
/**
 * In this example, the interrupt *nINT* is used. It is triggered every time
 * new data are available (INT_DATA_RDY_USED) or exceed defined thresholds
 * (INT_THRESHOLD_USED). In this case, the user has to define an interrupt
 * handler that fetches the data directly or triggers a task, that is waiting
 * to fetch the data. In this example, a task is defined which suspends itself
 * in each cycle to wait for fetching the data. The task is resumed by the
 * the interrupt handler.
 */

TaskHandle_t nINT_task;

// User task that fetches the sensor values.

void user_task_interrupt (void *pvParameters)
{
    uint16_t tvoc;
    uint16_t eco2;

    while (1)
    {
        // task suspends itself and waits to be resumed by interrupt handler
        vTaskSuspend (NULL);

        // after resume get the results and do something with them
        if (ccs811_get_results (sensor, &tvoc, &eco2, 0, 0))
            printf("%.3f CCS811 Sensor interrupt: TVOC %d ppb, eCO2 %d ppm\n",
                   (double)sdk_system_get_time()*1e-3, tvoc, eco2);
    }
}

// Interrupt handler which resumes user_task_interrupt on interrupt

static void IRAM nINT_handler(uint8_t gpio)
{
    xTaskResumeFromISR (nINT_task);
}

#else // !INT_USED

/*
 * In this example, user task fetches the sensor values every seconds.
 */

void user_task_periodic(void *pvParameters)
{
    uint16_t tvoc;
    uint16_t eco2;

    TickType_t last_wakeup = xTaskGetTickCount();

    while (1)
    {
        // get environmental data from another sensor and set them
        // ccs811_set_environmental_data (sensor, 25.3, 47.8);

        // get the results and do something with them
        if (ccs811_get_results (sensor, &tvoc, &eco2, 0, 0))
            printf("%.3f CCS811 Sensor periodic: TVOC %d ppb, eCO2 %d ppm\n",
                   (double)sdk_system_get_time()*1e-3, tvoc, eco2);

        // passive waiting until 1 second is over
        vTaskDelayUntil(&last_wakeup, 1000 / portTICK_PERIOD_MS);
    }
}

#endif // INT_USED

/* -- main program ------------------------------------------------- */

void user_init(void)
{
    // Set UART Parameter.
    uart_set_baud(0, 115200);
    // Give the UART some time to settle
    vTaskDelay(1);

    /** -- MANDATORY PART -- */

    // init all I2C bus interfaces at which CCS811 sensors are connected
    i2c_init (I2C_BUS, I2C_SCL_PIN, I2C_SDA_PIN, I2C_FREQ);
    
    // longer clock stretching is required for CCS811
    i2c_set_clock_stretch (I2C_BUS, CCS811_I2C_CLOCK_STRETCH);

    // init the sensor with slave address CCS811_I2C_ADDRESS_1 connected I2C_BUS.
    sensor = ccs811_init_sensor (I2C_BUS, CCS811_I2C_ADDRESS_1);

    if (sensor)
    {
        #if !defined (INT_USED)

        // create a periodic task that uses the sensor
        xTaskCreate(user_task_periodic, "user_task_periodic", TASK_STACK_DEPTH, NULL, 2, NULL);

        #else // INT_USED

        // create a task that is resumed by interrupt handler to use the sensor
        xTaskCreate(user_task_interrupt, "user_task_interrupt", TASK_STACK_DEPTH, NULL, 2, &nINT_task);

        // activate the interrupt for nINT_PIN and set the interrupt handler
        gpio_enable(nINT_PIN, GPIO_INPUT);
        gpio_set_interrupt(nINT_PIN, GPIO_INTTYPE_EDGE_NEG, nINT_handler);

        #ifdef INT_DATA_RDY_USED
        // enable the data ready interrupt
        ccs811_enable_interrupt (sensor, true);
        #else // INT_THRESHOLD_USED
        // set threshold parameters and enable threshold interrupt mode
        ccs811_set_eco2_thresholds (sensor, 600, 1100, 40);
        #endif

        #endif  // !defined(INT_USED)

        // start periodic measurement with one measurement per second
        ccs811_set_mode (sensor, ccs811_mode_1s);
    }
    else
        printf("Could not initialize the CCS811 sensor\n");
}

