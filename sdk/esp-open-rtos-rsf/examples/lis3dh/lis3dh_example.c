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
 *   | ESP8266 / ESP32 |   | LIS3DH   |
 *   |                 |   |          |
 *   |   GPIO 14 (SCL) ----> SCL      |
 *   |   GPIO 13 (SDA) <---> SDA      |
 *   |   GPIO 5        <---- INT1     |
 *   +-----------------+   +----------+
 *
 *   SPI   
 *
 *   +-----------------+   +----------+      +-----------------+   +----------+
 *   | ESP8266         |   | LIS3DH   |      | ESP32           |   | LIS3DH   |
 *   |                 |   |          |      |                 |   |          |
 *   |   GPIO 14 (SCK) ----> SCK      |      |   GPIO 16 (SCK) ----> SCK      |
 *   |   GPIO 13 (MOSI)----> SDI      |      |   GPIO 17 (MOSI)----> SDI      |
 *   |   GPIO 12 (MISO)<---- SDO      |      |   GPIO 18 (MISO)<---- SDO      |
 *   |   GPIO 2  (CS)  ----> CS       |      |   GPIO 19 (CS)  ----> CS       |
 *   |   GPIO 5        <---- INT1     |      |   GPIO 5        <---- INT1     |
 *   +-----------------+    +---------+      +-----------------+   +----------+
 */

/* -- use following constants to define the example mode ----------- */

// #define SPI_USED     // SPI interface is used, otherwise I2C
// #define FIFO_MODE    // multiple sample read mode
// #define INT_DATA     // data interrupts used (data ready and FIFO status)
// #define INT_EVENT    // inertial event interrupts used (wake-up, free fall or 6D/4D orientation)
// #define INT_CLICK    // click detection interrupts used

#if defined(INT_DATA) || defined(INT_EVENT) || defined(INT_CLICK)
#define INT_USED
#endif

/* -- includes ----------------------------------------------------- */

#include "lis3dh.h"

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
#define INT1_PIN      5
#define INT2_PIN      4

/* -- user tasks --------------------------------------------------- */

static lis3dh_sensor_t* sensor;

/**
 * Common function used to get sensor data.
 */
void read_data ()
{
    #ifdef FIFO_MODE

    lis3dh_float_data_fifo_t fifo;

    if (lis3dh_new_data (sensor))
    {
        uint8_t num = lis3dh_get_float_data_fifo (sensor, fifo);

        printf("%.3f LIS3DH num=%d\n", (double)sdk_system_get_time()*1e-3, num);

        for (int i=0; i < num; i++)
            // max. full scale is +-16 g and best resolution is 1 mg, i.e. 5 digits
            printf("%.3f LIS3DH (xyz)[g] ax=%+7.3f ay=%+7.3f az=%+7.3f\n",
                   (double)sdk_system_get_time()*1e-3, 
                   fifo[i].ax, fifo[i].ay, fifo[i].az);
    }

    #else

    lis3dh_float_data_t  data;

    if (lis3dh_new_data (sensor) &&
        lis3dh_get_float_data (sensor, &data))
        // max. full scale is +-16 g and best resolution is 1 mg, i.e. 5 digits
        printf("%.3f LIS3DH (xyz)[g] ax=%+7.3f ay=%+7.3f az=%+7.3f\n",
               (double)sdk_system_get_time()*1e-3, 
                data.ax, data.ay, data.az);
        
    #endif // FIFO_MODE
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
    {
        if (xQueueReceive(gpio_evt_queue, &gpio_num, portMAX_DELAY))
        {
            lis3dh_int_data_source_t  data_src  = {};
            lis3dh_int_event_source_t event_src = {};
            lis3dh_int_click_source_t click_src = {};

            // get the source of the interrupt and reset *INTx* signals
            #ifdef INT_DATA
            lis3dh_get_int_data_source  (sensor, &data_src);
            #endif
            #ifdef INT_EVENT
            lis3dh_get_int_event_source (sensor, &event_src, lis3dh_int_event1_gen);
            #endif
            #ifdef INT_CLICK
            lis3dh_get_int_click_source (sensor, &click_src);
            #endif
    
            // in case of DRDY interrupt or inertial event interrupt read one data sample
            if (data_src.data_ready)
                read_data ();
   
            // in case of FIFO interrupts read the whole FIFO
            else  if (data_src.fifo_watermark || data_src.fifo_overrun)
                read_data ();
    
            // in case of event interrupt
            else if (event_src.active)
            {
                printf("%.3f LIS3DH ", (double)sdk_system_get_time()*1e-3);
                if (event_src.x_low)  printf("x is lower than threshold\n");
                if (event_src.y_low)  printf("y is lower than threshold\n");
                if (event_src.z_low)  printf("z is lower than threshold\n");
                if (event_src.x_high) printf("x is higher than threshold\n");
                if (event_src.y_high) printf("y is higher than threshold\n");
                if (event_src.z_high) printf("z is higher than threshold\n");
            }

            // in case of click detection interrupt   
            else if (click_src.active)
               printf("%.3f LIS3DH %s\n", (double)sdk_system_get_time()*1e-3, 
                      click_src.s_click ? "single click" : "double click");
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
    sensor = lis3dh_init_sensor (SPI_BUS, 0, SPI_CS_GPIO);
    
    #else

    // init all I2C bus interfaces at which LIS3DH  sensors are connected
    i2c_init (I2C_BUS, I2C_SCL_PIN, I2C_SDA_PIN, I2C_FREQ);
    
    // init the sensor with slave address LIS3DH_I2C_ADDRESS_1 connected to I2C_BUS.
    sensor = lis3dh_init_sensor (I2C_BUS, LIS3DH_I2C_ADDRESS_1, 0);

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

        // configure interupt pins for *INT1* and *INT2* signals and set the interrupt handler
        gpio_enable(INT1_PIN, GPIO_INPUT);
        gpio_set_interrupt(INT1_PIN, GPIO_INTTYPE_EDGE_POS, int_signal_handler);

        #endif  // INT_USED
        
        /** -- SENSOR CONFIGURATION PART --- */

        // set polarity of INT signals if necessary
        // lis3dh_config_int_signals (sensor, lis3dh_high_active);

        #ifdef INT_DATA
        // enable data interrupts on INT1 (data ready or FIFO status interrupts)
        // data ready and FIFO status interrupts must not be enabled at the same time
        #ifdef FIFO_MODE
        lis3dh_enable_int (sensor, lis3dh_int_fifo_overrun  , lis3dh_int1_signal, true);
        lis3dh_enable_int (sensor, lis3dh_int_fifo_watermark, lis3dh_int1_signal, true);
        #else
        lis3dh_enable_int (sensor, lis3dh_int_data_ready, lis3dh_int1_signal, true);
        #endif // FIFO_MODE
        #endif // INT_DATA
        
        #ifdef INT_EVENT
        // enable data interrupts on INT1 
        lis3dh_int_event_config_t event_config;
    
        event_config.mode = lis3dh_wake_up;
        // event_config.mode = lis3dh_free_fall;
        // event_config.mode = lis3dh_6d_movement;
        // event_config.mode = lis3dh_6d_position;
        // event_config.mode = lis3dh_4d_movement;
        // event_config.mode = lis3dh_4d_position;
        event_config.threshold = 10;
        event_config.x_low_enabled  = false;
        event_config.x_high_enabled = true;
        event_config.y_low_enabled  = false;
        event_config.y_high_enabled = true;
        event_config.z_low_enabled  = false;
        event_config.z_high_enabled = true;
        event_config.duration = 0;
        event_config.latch = true;
        
        lis3dh_set_int_event_config (sensor, &event_config, lis3dh_int_event1_gen);
        lis3dh_enable_int (sensor, lis3dh_int_event1, lis3dh_int1_signal, true);
        #endif // INT_EVENT

        #ifdef INT_CLICK
        // enable click interrupt on INT1
        lis3dh_int_click_config_t click_config;
        
        click_config.threshold = 10;
        click_config.x_single = false;
        click_config.x_double = false;        
        click_config.y_single = false;
        click_config.y_double = false;        
        click_config.z_single = true;
        click_config.z_double = false;
        click_config.latch = true;
        click_config.time_limit   = 1;
        click_config.time_latency = 1;
        click_config.time_window  = 3;
        
        lis3dh_set_int_click_config (sensor, &click_config);
        lis3dh_enable_int (sensor, lis3dh_int_click, lis3dh_int1_signal, true);
        #endif // INT_CLICK

        #ifdef FIFO_MODE
        // clear FIFO and activate FIFO mode if needed
        lis3dh_set_fifo_mode (sensor, lis3dh_bypass,  0, lis3dh_int1_signal);
        lis3dh_set_fifo_mode (sensor, lis3dh_stream, 10, lis3dh_int1_signal);
        #endif

        // configure HPF and reset the reference by dummy read
        lis3dh_config_hpf (sensor, lis3dh_hpf_normal, 0, true, true, true, true);
        lis3dh_get_hpf_ref (sensor);
        
        // enable ADC inputs and temperature sensor for ADC input 3
        lis3dh_enable_adc (sensor, true, true);
        
        // LAST STEP: Finally set scale and mode to start measurements
        lis3dh_set_scale(sensor, lis3dh_scale_2_g);
        lis3dh_set_mode (sensor, lis3dh_odr_10, lis3dh_high_res, true, true, true);

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
        printf("Could not initialize LIS3DH sensor\n");
}

