/**
 * Simple example with one sensor connected to I2C or SPI. It demonstrates the
 * different approaches to fetch the data. Either one of the interrupt signals
 * is used or new data are fetched periodically.
 *
 * Harware configuration:
 *
 *   I2C
 *
 *   +-----------------+   +----------+
 *   | ESP8266 / ESP32 |   | LIS3MDL  |
 *   |                 |   |          |
 *   |   GPIO 14 (SCL) ----> SCL      |
 *   |   GPIO 13 (SDA) <---> SDA      |
 *   |   GPIO 5        <---- INT      |
 *   |   GPIO 4        <---- DRDY     |
 *   +-----------------+   +----------+
 *
 *   SPI   
 *
 *   +-----------------+   +----------+      +-----------------+   +----------+
 *   | ESP8266         |   | LIS3MDL  |      | ESP32           |   | LIS3MDL  |
 *   |                 |   |          |      |                 |   |          |
 *   |   GPIO 14 (SCK) ----> SCK      |      |   GPIO 16 (SCK) ----> SCK      |
 *   |   GPIO 13 (MOSI)----> SDI      |      |   GPIO 17 (MOSI)----> SDI      |
 *   |   GPIO 12 (MISO)<---- SDO      |      |   GPIO 18 (MISO)<---- SDO      |
 *   |   GPIO 2  (CS)  ----> CS       |      |   GPIO 19 (CS)  ----> CS       |
 *   |   GPIO 5        <---- INT      |      |   GPIO 5        <---- INT      |
 *   |   GPIO 4        <---- DRDY     |      |   GPIO 4        <---- DRDY     |
 *   +-----------------+    +---------+      +-----------------+   +----------+
 */

/* -- use following constants to define the example mode ----------- */

// #define SPI_USED     // if defined SPI is used, otherwise I2C
// #define INT_DATA     // data ready interrupt used
// #define INT_THRESH   // threshold interrupt used

#if defined(INT_DATA) || defined(INT_THRESH)
#define INT_USED
#endif

/* -- includes ----------------------------------------------------- */

#include "lis3mdl.h"

/** -- platform dependent definitions ------------------------------ */

#ifdef ESP_PLATFORM  // ESP32 (ESP-IDF)

// user task stack depth for ESP32
#define TASK_STACK_DEPTH 2048

// SPI interface definitions for ESP32
#define SPI_BUS       HSPI_HOST
#define SPI_SCK_GPIO  16
#define SPI_MOSI_GPIO 17
#define SPI_MISO_GPIO 18
#define SPI_CS_GPIO   19

#else  // ESP8266 (esp-open-rtos)

// user task stack depth for ESP8266
#define TASK_STACK_DEPTH 256

// SPI interface definitions for ESP8266
#define SPI_BUS       1
#define SPI_SCK_GPIO  14
#define SPI_MOSI_GPIO 13
#define SPI_MISO_GPIO 12
#define SPI_CS_GPIO   2   // GPIO 15, the default CS of SPI bus 1, can't be used

#endif  // ESP_PLATFORM

// I2C interface defintions for ESP32 and ESP8266
#define I2C_BUS       0
#define I2C_SCL_PIN   14
#define I2C_SDA_PIN   13
#define I2C_FREQ      I2C_FREQ_100K

// interrupt GPIOs defintions for ESP8266 and ESP32
#define PIN_INT       5
#define PIN_DRDY      4

/* -- user tasks --------------------------------------------------- */

static lis3mdl_sensor_t* sensor;

/**
 * Common function used to get sensor data.
 */
void read_data ()
{
    lis3mdl_float_data_t  data;

    if (lis3mdl_new_data (sensor) &&
        lis3mdl_get_float_data (sensor, &data))
        // max. full scale is +-16 g and best resolution is 1 mg, i.e. 5 digits
        printf("%.3f LIS3MDL (xyz)[Gs] mx=%+7.3f my=%+7.3f mz=%+7.3f\n",
               (double)sdk_system_get_time()*1e-3, 
                data.mx, data.my, data.mz);
}


#ifdef INT_USED
/**
 * In this case, any of the possible interrupts on interrupt signal *INT1* is
 * used to fetch the data.
 *
 * When interrupts are used, the user has to define interrupt handlers that
 * either fetches the data directly or triggers a task which is waiting to
 * fetch the data. In this example, the interrupt handler sends an event to
 * a waiting task to trigger the data gathering.
 */

static QueueHandle_t gpio_evt_queue = NULL;

// User task that fetches the sensor values.

void user_task_interrupt (void *pvParameters)
{
    uint8_t gpio_num;

    while (1)
        if (xQueueReceive(gpio_evt_queue, &gpio_num, portMAX_DELAY))
        {
            if (gpio_num == PIN_DRDY)
            {
                read_data ();
            }
            else if (gpio_num == PIN_INT)
            {
                lis3mdl_int_source_t int_src;

                // get the source of the interrupt and reset INT signals
                lis3mdl_get_int_source (sensor, &int_src);
    
                // in case of DRDY interrupt or activity interrupt read one data sample
                if (int_src.active)
                    read_data ();
            }
        }
}

// Interrupt handler which resumes user_task_interrupt on interrupt

void IRAM int_signal_handler (uint8_t gpio)
{
    // send an event with GPIO to the interrupt user task
    xQueueSendFromISR(gpio_evt_queue, &gpio, NULL);
}

#else // !INT_USED

/*
 * In this example, user task fetches the sensor values every seconds.
 */

void user_task_periodic(void *pvParameters)
{
    vTaskDelay (100/portTICK_PERIOD_MS);
    
    while (1)
    {
        // read sensor data
        read_data ();
        
        // passive waiting until 1 second is over
        vTaskDelay(100/portTICK_PERIOD_MS);
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

    #ifdef SPI_USED

    // init the sensor connnected to SPI
    spi_bus_init (SPI_BUS, SPI_SCK_GPIO, SPI_MISO_GPIO, SPI_MOSI_GPIO);

    // init the sensor connected to SPI_BUS with SPI_CS_GPIO as chip select.
    sensor = lis3mdl_init_sensor (SPI_BUS, 0, SPI_CS_GPIO);
    
    #else

    // init all I2C bus interfaces at which LIS3MDL  sensors are connected
    i2c_init (I2C_BUS, I2C_SCL_PIN, I2C_SDA_PIN, I2C_FREQ);
    
    // init the sensor with slave address LIS3MDL_I2C_ADDRESS_2 connected to I2C_BUS.
    sensor = lis3mdl_init_sensor (I2C_BUS, LIS3MDL_I2C_ADDRESS_2, 0);

    #endif
    
    if (sensor)
    {
        #ifdef INT_USED

        /** --- INTERRUPT CONFIGURATION PART ---- */
        
        // Interrupt configuration has to be done before the sensor is set
        // into measurement mode to avoid losing interrupts

        // create an event queue to send interrupt events from interrupt
        // handler to the interrupt task
        gpio_evt_queue = xQueueCreate(10, sizeof(uint8_t));

        // configure interupt pins for *INT* and *DRDY* signals and set the interrupt handler
        gpio_enable(PIN_INT , GPIO_INPUT);
        gpio_enable(PIN_DRDY, GPIO_INPUT);
        gpio_set_interrupt(PIN_INT , GPIO_INTTYPE_EDGE_POS, int_signal_handler);
        gpio_set_interrupt(PIN_DRDY, GPIO_INTTYPE_EDGE_POS, int_signal_handler);

        #endif  // !defined(INT_USED)
        
        // -- SENSOR CONFIGURATION PART ---

        // Interrupt configuration has to be done before the sensor is set
        // into measurement mode
        
        #ifdef INT_THRESH
        // enable threshold interrupts on INT1 
        lis3mdl_int_config_t int_config;
    
        int_config.threshold   = 1000;
        int_config.x_enabled   = true;
        int_config.y_enabled   = true;
        int_config.z_enabled   = true;
        int_config.latch       = true;
        int_config.signal_level= lis3mdl_high_active;
        
        lis3mdl_set_int_config (sensor, &int_config);
        #endif // INT_THRESH

        // LAST STEP: Finally set scale and mode to start measurements
        lis3mdl_set_scale(sensor, lis3mdl_scale_4_Gs);
        lis3mdl_set_mode (sensor, lis3mdl_lpm_10);

        /** -- TASK CREATION PART --- */

        // must be done last to avoid concurrency situations with the sensor
        // configuration part

        #ifdef INT_USED

        // create a task that is triggered only in case of interrupts to fetch the data
        xTaskCreate(user_task_interrupt, "user_task_interrupt", TASK_STACK_DEPTH, NULL, 2, NULL);
        
        #else // INT_USED

        // create a user task that fetches data from sensor periodically
        xTaskCreate(user_task_periodic, "user_task_periodic", TASK_STACK_DEPTH, NULL, 2, NULL);

        #endif
    }
    else
        printf("Could not initialize LIS3MDL sensor\n");
}

