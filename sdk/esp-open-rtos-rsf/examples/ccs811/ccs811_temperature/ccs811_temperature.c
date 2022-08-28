/**
 * Simple example with one sensor connected to I2C bus 0. It demonstrates
 * how to use CCS811 with an external NTC thermistor to determine ambient
 * temperature.
 *
 * Harware configuration:
 *
 * +------------------------+    +--------+
 * | ESP8266  Bus 0         |    | CCS811 |
 * |          GPIO 14 (SCL) >----> SCL    |
 * |          GPIO 13 (SDA) <----> SDA    |
 * |          GND           -----> /WAKE  |
 * +------------------------+    +--------+
 */

#include "espressif/esp_common.h"
#include "esp/uart.h"
#include "i2c/i2c.h"

#include "FreeRTOS.h"
#include <task.h>

#include <math.h>

// include CCS811 driver
#include "ccs811/ccs811.h"

// define I2C interfaces at which CCS811 sensors can be connected
#define I2C_BUS       0
#define I2C_SCL_PIN   14
#define I2C_SDA_PIN   13


static ccs811_sensor_t* sensor;


/*
 * In this example, user task fetches the sensor values every seconds.
 */

// parameters of the Adafruit CCS811 Air Quality Sensor Breakout
#define CCS811_R_REF        100000
#define CCS811_R_NTC        10000
#define CCS811_R_NTC_TEMP   25
#define CCS811_BCONSTANT    3380

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

        // get NTC resistance
        uint32_t r_ntc = ccs811_get_ntc_resistance (sensor, CCS811_R_REF);
    
        // calculation of temperature from application note ams AN000372
        double ntc_temp;
        ntc_temp  = log((double)r_ntc / CCS811_R_NTC);      // 1
        ntc_temp /= CCS811_BCONSTANT;                       // 2
        ntc_temp += 1.0 / (CCS811_R_NTC_TEMP + 273.15);     // 3
        ntc_temp  = 1.0 / ntc_temp;                         // 4      
        ntc_temp -= 273.15;                                 // 5

        printf("%.3f CCS811 Sensor temperature: R_NTC %u Ohm, T %f °C\n", 
               (double)sdk_system_get_time()*1e-3, r_ntc, ntc_temp);

        // passive waiting until 1 second is over
        vTaskDelayUntil(&last_wakeup, 1100 / portTICK_PERIOD_MS);
    }
}


void user_init(void)
{
    // set UART Parameter
    uart_set_baud(0, 115200);
    // give the UART some time to settle
    sdk_os_delay_us(500);
    
    /** -- MANDATORY PART -- */
    
    // init all I2C bus interfaces at which CCS811 sensors are connected
    i2c_init(I2C_BUS, I2C_SCL_PIN, I2C_SDA_PIN, I2C_FREQ_100K);

    // longer clock stretching is required for CCS811
    i2c_set_clock_stretch (I2C_BUS, CCS811_I2C_CLOCK_STRETCH);

    // init the sensor with slave address CCS811_I2C_ADDRESS_1 connected I2C_BUS.
    sensor = ccs811_init_sensor (I2C_BUS, CCS811_I2C_ADDRESS_1);

    if (sensor)
    {
        #if defined(INT_DATA_RDY_USED) || defined(INT_THRESHOLD_USED)

        // create a task that is resumed by interrupt handler to use the sensor
        xTaskCreate(user_task_interrupt, "user_task_interrupt", 256, NULL, 2, &nINT_task);

        // set the GPIO and interrupt handler for *nINT* interrupt
        gpio_set_interrupt(INT_GPIO, GPIO_INTTYPE_EDGE_NEG, nINT_handler);

        #ifdef INT_DATA_RDY_USED
        // enable the data ready interrupt
        ccs811_enable_interrupt (sensor, true);
        #else
        // set threshold parameters and enable threshold interrupt mode
        ccs811_set_eco2_thresholds (sensor, 600, 1100, 40);
        #endif
       
        #else
         
        // create a periodic task that uses the sensor
        xTaskCreate(user_task_periodic, "user_task_periodic", 256, NULL, 2, NULL);

        #endif
        
        // start periodic measurement with one measurement per second
        ccs811_set_mode (sensor, ccs811_mode_1s);
    }
}

