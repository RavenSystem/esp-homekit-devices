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
 *   | ESP8266 / ESP32 |   | LSM303D  |
 *   |                 |   |          |
 *   |   GPIO 14 (SCL) ----> SCL      |
 *   |   GPIO 13 (SDA) <---> SDA      |
 *   |   GPIO 5        <---- INT1     |
 *   |   GPIO 4        <---- INT2     |
 *   +-----------------+   +----------+
 *
 *   SPI   
 *
 *   +-----------------+   +----------+      +-----------------+   +----------+
 *   | ESP8266         |   | LSM303D  |      | ESP32           |   | LSM303D  |
 *   |                 |   |          |      |                 |   |          |
 *   |   GPIO 14 (SCK) ----> SCK      |      |   GPIO 16 (SCK) ----> SCK      |
 *   |   GPIO 13 (MOSI)----> SDI      |      |   GPIO 17 (MOSI)----> SDI      |
 *   |   GPIO 12 (MISO)<---- SDO      |      |   GPIO 18 (MISO)<---- SDO      |
 *   |   GPIO 2  (CS)  ----> CS       |      |   GPIO 19 (CS)  ----> CS       |
 *   |   GPIO 5        <---- INT1     |      |   GPIO 5        <---- INT1     |
 *   |   GPIO 4        <---- INT2     |      |   GPIO 4        <---- INT2     |
 *   +-----------------+    +---------+      +-----------------+   +----------+
 */

/* -- use following constants to define the example mode ----------- */

// #define SPI_USED   // SPI interface is used, otherwise I2C
// #define FIFO_MODE  // multiple sample read mode
// #define TEMP_USED  // temperature sensor used
// #define INT_DATA   // data interrupts used (data ready and FIFO status)
// #define INT_EVENT  // inertial event interrupts used (axis movement or 6D/4D orientation)
// #define INT_CLICK  // click detection interrupts used
// #define INT_THRESH // magnetic value exceeds threshold interrupt used

#if defined(INT_DATA) || defined(INT_EVENT) || defined(INT_CLICK) || defined(INT_THRESH)
#define INT_USED
#endif

/* -- includes ----------------------------------------------------- */

#include "lsm303d.h"

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

static lsm303d_sensor_t* sensor;

/**
 * Common function used to get sensor data.
 */
void read_data ()
{
    #ifdef FIFO_MODE

    lsm303d_float_a_data_fifo_t fifo;

    // test for new accelerator data data
    if (lsm303d_new_a_data (sensor))
    {
        // fetch the accelerator data stored in FIFO
        uint8_t num = lsm303d_get_float_a_data_fifo (sensor, fifo);

        printf("%.3f LSM303D num=%d\n", (double)sdk_system_get_time()*1e-3, num);

        for (int i=0; i < num; i++)
            // max. full scale is +-16 g and best resolution is 1 mg, i.e. 5 digits
            printf("%.3f LSM303D (xyz)[g]  ax=%+7.3f ay=%+7.3f az=%+7.3f\n",
                   (double)sdk_system_get_time()*1e-3, 
                   fifo[i].ax, fifo[i].ay, fifo[i].az);
    }

    #else

    lsm303d_float_a_data_t  a_data;

    // test for new accelerator data and fetch them
    if (lsm303d_new_a_data (sensor) &&
        lsm303d_get_float_a_data (sensor, &a_data))
        // max. full scale is +-16 g and best resolution is 1 mg, i.e. 5 digits
        printf("%.3f LSM303D (xyz)[g]  ax=%+7.3f ay=%+7.3f az=%+7.3f\n",
               (double)sdk_system_get_time()*1e-3, 
                a_data.ax, a_data.ay, a_data.az);
        
    #endif // FIFO_MODE

    lsm303d_float_m_data_t  m_data;

    // test for new magnetometer data and fetch them
    if (lsm303d_new_m_data (sensor) &&
        lsm303d_get_float_m_data (sensor, &m_data))
        // max. full scale is +-12 Gs and best resolution is 1 mGs, i.e. 5 digits
        printf("%.3f LSM303D (xyz)[Gs] mx=%+7.3f my=%+7.3f mz=%+7.3f\n",
               (double)sdk_system_get_time()*1e-3, 
                m_data.mx, m_data.my, m_data.mz);

    #ifdef TEMP_USED
    float temp = lsm303d_get_temperature (sensor);
    
    printf("%.3f LSM303D (tmp)[°C] %+7.3f\n", (double)sdk_system_get_time()*1e-3, temp);
    #endif
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
    uint8_t gpio;

    while (1)
    {
        if (xQueueReceive(gpio_evt_queue, &gpio, portMAX_DELAY))
        {
            lsm303d_int_data_source_t      data_src   = {};
            lsm303d_int_event_source_t     event_src  = {};
            lsm303d_int_click_source_t     click_src  = {};
            lsm303d_int_m_thresh_source_t  thresh_src = {};

            // get the source of the interrupt that reset *INTx* signals
            #ifdef INT_DATA
            lsm303d_get_int_data_source    (sensor, &data_src);
            #endif
            #ifdef INT_THRESH
            lsm303d_get_int_m_thresh_source(sensor, &thresh_src);
            #endif
            #ifdef INT_EVENT
            lsm303d_get_int_event_source   (sensor, &event_src, lsm303d_int_event1_gen);
            #endif
            #ifdef INT_CLICK
            lsm303d_get_int_click_source   (sensor, &click_src);
            #endif

            // in case of DRDY interrupt
            if (data_src.a_data_ready || data_src.m_data_ready)
                read_data ();
                
            // in case of FIFO interrupts read the whole FIFO
            else if (data_src.fifo_thresh || data_src.fifo_overrun)
                read_data ();
    
            // in case of magnetic threshold interrupt
            else if (thresh_src.active)
            {
                printf("%.3f LSM303D ", (double)sdk_system_get_time()*1e-3);
                if (thresh_src.x_pos) printf("x exceeds threshold on positive side\n");
                if (thresh_src.y_pos) printf("y exceeds threshold on positive side\n");
                if (thresh_src.z_pos) printf("z exceeds threshold on positive side\n");
                if (thresh_src.x_neg) printf("x exceeds threshold on negative side\n");
                if (thresh_src.y_neg) printf("y exceeds threshold on negative side\n");
                if (thresh_src.z_neg) printf("z exceeds threshold on negative side\n");
            }
            
            // in case of event interrupt
            else if (event_src.active)
            {
                printf("%.3f LSM303D ", (double)sdk_system_get_time()*1e-3);
                if (event_src.x_low)  printf("x is lower than threshold\n");
                if (event_src.y_low)  printf("y is lower than threshold\n");
                if (event_src.z_low)  printf("z is lower than threshold\n");
                if (event_src.x_high) printf("x is higher than threshold\n");
                if (event_src.y_high) printf("y is higher than threshold\n");
                if (event_src.z_high) printf("z is higher than threshold\n");
            }

            // in case of click detection interrupt   
            else if (click_src.active)
               printf("%.3f LSM303D %s\n", (double)sdk_system_get_time()*1e-3, 
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
        vTaskDelay(200/portTICK_PERIOD_MS);
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

    // init the SPI interface at which LMS303D sensors are connected
    spi_bus_init (SPI_BUS, SPI_SCK_GPIO, SPI_MISO_GPIO, SPI_MOSI_GPIO);

    // init the sensor connected to SPI_BUS with SPI_CS_GPIO as chip select.
    sensor = lsm303d_init_sensor (SPI_BUS, 0, SPI_CS_GPIO);
    
    #else

    // init all I2C busses at which LSM303D sensors are connected
    i2c_init (I2C_BUS, I2C_SCL_PIN, I2C_SDA_PIN, I2C_FREQ);
    
    // init the sensor with slave address LSM303D_I2C_ADDRESS_2 connected to I2C_BUS.
    sensor = lsm303d_init_sensor (I2C_BUS, LSM303D_I2C_ADDRESS_2, 0);

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
        gpio_enable(INT2_PIN, GPIO_INPUT);
        gpio_set_interrupt(INT1_PIN, GPIO_INTTYPE_EDGE_POS, int_signal_handler);
        gpio_set_interrupt(INT2_PIN, GPIO_INTTYPE_EDGE_POS, int_signal_handler);

        #endif  // INT_USED
        
        /** -- SENSOR CONFIGURATION PART --- */

        // set the type of INTx signals if necessary
        // lsm303d_config_int_signals (sensor, lsm303d_push_pull);

        #ifdef INT_DATA
        // enable data interrupts on *INT2* (data ready or FIFO overrun and FIFO threshold)
        // data ready and FIFO status interrupts must not be enabled at the same time
        #ifdef FIFO_MODE
        lsm303d_enable_int (sensor, lsm303d_int_fifo_overrun, lsm303d_int2_signal, true);
        lsm303d_enable_int (sensor, lsm303d_int_fifo_thresh , lsm303d_int2_signal, true);
        #else
        lsm303d_enable_int (sensor, lsm303d_int_a_data_ready, lsm303d_int2_signal, true);
        lsm303d_enable_int (sensor, lsm303d_int_m_data_ready, lsm303d_int2_signal, true);
        #endif // FIFO_MODE
        #endif // INT_DATA
        
        #ifdef INT_THRESH
        // enable magnetic threshold interrupts on signal *INT1* 
        lsm303d_int_m_thresh_config_t m_thresh_config;
    
        m_thresh_config.threshold    = 2000;
        m_thresh_config.x_enabled    = true;
        m_thresh_config.y_enabled    = true;
        m_thresh_config.z_enabled    = true;
        m_thresh_config.latch        = true;
        m_thresh_config.signal_level = lsm303d_high_active;
        
        lsm303d_set_int_m_thresh_config (sensor, &m_thresh_config);
        lsm303d_enable_int (sensor, lsm303d_int_m_thresh, lsm303d_int1_signal, true);
        #endif // INT_THRESH

        #ifdef INT_EVENT
        // enable inertial event interrupts on *INT1*
        lsm303d_int_event_config_t event_config;
    
        event_config.mode = lsm303d_or;       // axes movement wake-up
        // event_config.mode = lsm303d_and;   // free fall
        // event_config.mode = lsm303d_6d_movement;
        // event_config.mode = lsm303d_6d_position;
        // event_config.mode = lsm303d_4d_movement;
        // event_config.mode = lsm303d_4d_position;
        event_config.threshold = 50;
        event_config.x_low_enabled  = false;
        event_config.x_high_enabled = true;
        event_config.y_low_enabled  = false;
        event_config.y_high_enabled = true;
        event_config.z_low_enabled  = false;
        event_config.z_high_enabled = true;
        event_config.duration = 0;
        event_config.latch = true;
        
        lsm303d_set_int_event_config (sensor, &event_config, lsm303d_int_event1_gen);
        lsm303d_enable_int (sensor, lsm303d_int_event1, lsm303d_int1_signal, true);
        #endif // INT_EVENT

        #ifdef INT_CLICK
        // enable single click interrupt for z-axis on signal *INT1*
        lsm303d_int_click_config_t click_config;
        
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
        
        lsm303d_set_int_click_config (sensor, &click_config);
        lsm303d_enable_int (sensor, lsm303d_int_click, lsm303d_int1_signal, true);
        #endif // INT_CLICK

        #ifdef FIFO_MODE
        // clear the FIFO
        lsm303d_set_fifo_mode (sensor, lsm303d_bypass, 0);

        // activate the FIFO with a threshold of 10 samples (max. 31); if 
        // interrupt *lsm303d_fifo_thresh* is enabled, an interrupt is
        // generated when the FIFO content exceeds this threshold, i.e., 
        // when 11 samples are stored in FIFO
        lsm303d_set_fifo_mode (sensor, lsm303d_stream, 10);
        #endif

        // configure HPF and implicitly reset the reference by a dummy read
        lsm303d_config_a_hpf (sensor, lsm303d_hpf_normal, true, true, true, true);
        
        #ifdef TEMP_USED
        // enable the temperature sensor
        lsm303d_enable_temperature (sensor, true);
        #endif
                
        // LAST STEP: Finally set scale and mode to start measurements
        lsm303d_set_a_scale(sensor, lsm303d_a_scale_2_g);
        lsm303d_set_m_scale(sensor, lsm303d_m_scale_4_Gs);
        lsm303d_set_a_mode (sensor, lsm303d_a_odr_12_5, lsm303d_a_aaf_bw_773, true, true, true);
        lsm303d_set_m_mode (sensor, lsm303d_m_odr_12_5, lsm303d_m_low_res, lsm303d_m_continuous);

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
        printf("Could not initialize LSM303D sensor\n");
}

