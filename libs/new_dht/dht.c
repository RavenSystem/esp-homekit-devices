/*
 * Part of esp-open-rtos
 * Copyright (C) 2016 Jonathan Hartsuiker (https://github.com/jsuiker)
 * BSD Licensed as described in the file LICENSE
 *
 */

#include "dht.h"
#include "string.h"

// DHT timer precision in microseconds
#define DHT_TIMER_INTERVAL          (2)
#define DHT_DATA_BITS               (40)

#define DHT_TIMEOUT_MS              (2000)

#ifdef ESP_PLATFORM

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "rom/ets_sys.h"

#define sdk_os_delay_us(time)       ets_delay_us(time)
#define gpio_read(level)            gpio_get_level(level)
#define gpio_write(gpio, level)     gpio_set_level(gpio, level)
#define PORT_ENTER_CRITICAL()       portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED; taskENTER_CRITICAL(&mux)
#define PORT_EXIT_CRITICAL()        taskEXIT_CRITICAL(&mux)

#else

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "esp/gpio.h"
#include <espressif/esp_misc.h>     // sdk_os_delay_us

#define PORT_ENTER_CRITICAL()       taskENTER_CRITICAL()
#define PORT_EXIT_CRITICAL()        taskEXIT_CRITICAL()

#endif

// #define DEBUG_DHT
#ifdef DEBUG_DHT
#define debug(fmt, ...) printf("%s" fmt "\n", "dht: ", ## __VA_ARGS__);
#else
#define debug(fmt, ...) /* (do nothing) */
#endif

/*
 *  Note:
 *  A suitable pull-up resistor should be connected to the selected GPIO line
 *
 *  __           ______          _______                              ___________________________
 *    \    A    /      \   C    /       \   DHT duration_data_low    /                           \
 *     \_______/   B    \______/    D    \__________________________/   DHT duration_data_high    \__
 *
 *
 *  Initializing communications with the DHT requires four 'phases' as follows:
 *
 *  Phase A - MCU pulls signal low for a period (see below)
 *  Phase B - MCU allows signal to float back up and waits 20-200us for DHT to pull it low
 *  Phase C - DHT pulls signal low for ~80us
 *  Phase D - DHT lets signal float back up for ~80us
 *
 *  The length of Phase A is dependent on the sensor type:
 *    DHT_TYPE_DHT11  : at least 18000 us
 *    DHT_TYPE_DHT22  : 800 us to 20000 us, typically 1100 us
 *    DHT_TYPE_SI7021 : 300 us to 500 us
 *   For a DHT21 use DHT_TYPE_DHT22
 *   For an AM2301/AM2302 use DHT_TYPE_SI7021
 *
 *  After this, the DHT transmits its first bit by holding the signal low for 50us
 *  and then letting it float back high for a period of time that depends on the data bit.
 *  duration_data_high is shorter than 50us for a logic '0' and longer than 50us for logic '1'.
 *
 *  There are a total of 40 data bits transmitted sequentially. These bits are read into a byte array
 *  of length 5.  The first and third bytes are humidity (%) and temperature (C), respectively.  Bytes 2 and 4
 *  are zero-filled and the fifth is a checksum such that:
 *
 *  byte_5 == (byte_1 + byte_2 + byte_3 + btye_4) & 0xFF
 *
*/

static SemaphoreHandle_t dht_lock = NULL;

/**
 * Wait specified time for pin to go to a specified state.
 * If timeout is reached and pin doesn't go to a requested state
 * false is returned.
 * The elapsed time is returned in pointer 'duration' if it is not NULL.
 */
static bool dht_await_pin_state(uint8_t pin, uint32_t timeout,
        bool expected_pin_state, uint32_t *duration)
{
#ifdef ESP_PLATFORM
    gpio_set_direction(pin, GPIO_MODE_INPUT);
#endif
    
    for (uint32_t i = 0; i < timeout; i += DHT_TIMER_INTERVAL) {
        // need to wait at least a single interval to prevent reading a jitter
        sdk_os_delay_us(DHT_TIMER_INTERVAL);
        if (gpio_read(pin) == expected_pin_state) {
            if (duration) {
                *duration = i;
            }
            return true;
        }
    }

    return false;
}

/**
 * Request data from DHT and read raw bit stream.
 * The function call should be protected from task switching.
 * Return false if error occurred.
 */
static inline bool dht_fetch_data(dht_sensor_type_t sensor_type, uint8_t pin, bool bits[DHT_DATA_BITS])
{
    uint32_t low_duration;
    uint32_t high_duration;

    // Phase 'A' pulling signal low to initiate read sequence
#ifdef ESP_PLATFORM
    gpio_set_direction(pin, GPIO_MODE_OUTPUT_OD);
#endif
    gpio_write(pin, 0);
    sdk_os_delay_us(sensor_type == DHT_TYPE_SI7021 ? 500 : 20000);
    gpio_write(pin, 1);
    
    // Step through Phase 'B', 200us
    if (!dht_await_pin_state(pin, sensor_type == DHT_TYPE_SI7021 ? 200 : 40, false, NULL)) {
        debug("Initialization error, problem in phase 'B'\n");
        return false;
    }
    
    // Step through Phase 'C', 88us
    if (!dht_await_pin_state(pin, 88, true, NULL)) {
        debug("Initialization error, problem in phase 'C'\n");
        return false;
    }

    // Step through Phase 'D', 88us
    if (!dht_await_pin_state(pin, 88, false, NULL)) {
        debug("Initialization error, problem in phase 'D'\n");
        return false;
    }

    // Read in each of the 40 bits of data...
    for (int i = 0; i < DHT_DATA_BITS; i++) {
        if (!dht_await_pin_state(pin, 65, true, &low_duration)) {
            debug("LOW bit timeout\n");
            return false;
        }
        if (!dht_await_pin_state(pin, 75, false, &high_duration)) {
            debug("HIGHT bit timeout\n");
            return false;
        }
        bits[i] = high_duration > low_duration;
    }
    return true;
}

/**
 * Pack two data bytes into single value and take into account sign bit.
 */
static inline int16_t dht_convert_data(dht_sensor_type_t sensor_type, uint8_t msb, uint8_t lsb)
{
    int16_t data;

    if (sensor_type == DHT_TYPE_DHT22 || sensor_type == DHT_TYPE_SI7021) {
        data = msb & 0x7F;
        data <<= 8;
        data |= lsb;
        if (msb & BIT(7)) {
            data = 0 - data;       // convert it to negative
        }
    }
    else {
        data = msb * 10;
    }

    return data;
}

bool dht_read_data(dht_sensor_type_t sensor_type, uint8_t pin, int16_t *humidity, int16_t *temperature)
{
    bool bits[DHT_DATA_BITS];
    uint8_t data[DHT_DATA_BITS / 8] = { 0 };
    bool result = false;
    
    if (xSemaphoreTake(dht_lock, pdMS_TO_TICKS(DHT_TIMEOUT_MS)) == pdTRUE) {
        
#ifdef ESP_PLATFORM
        gpio_set_direction(pin, GPIO_MODE_OUTPUT_OD);
        gpio_set_pull_mode(pin, false);
        gpio_sleep_set_pull_mode(pin, false);
#else
        gpio_enable(pin, GPIO_OUT_OPEN_DRAIN);
        gpio_set_pullup(pin, false, false);
#endif
        
        PORT_ENTER_CRITICAL();
        result = dht_fetch_data(sensor_type, pin, bits);
        PORT_EXIT_CRITICAL();
        
#ifdef ESP_PLATFORM
        gpio_reset_pin(pin);
        gpio_set_direction(pin, GPIO_MODE_DISABLE);
#else
        gpio_enable(pin, GPIO_INPUT);
#endif
        
        vTaskDelay(pdMS_TO_TICKS(50));
        
        xSemaphoreGive(dht_lock);
    }
    
    if (!result) {
        return false;
    }

    for (int i = 0; i < DHT_DATA_BITS; i++) {
        // Read each bit into 'result' byte array...
        data[i / 8] <<= 1;
        data[i / 8] |= bits[i];
    }

    if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
        debug("Checksum failed, invalid data received from sensor\n");
        return false;
    }

    *humidity = dht_convert_data(sensor_type, data[0], data[1]);
    *temperature = dht_convert_data(sensor_type, data[2], data[3]);

    debug("Sensor data: humidity=%d, temp=%d\n", *humidity, *temperature);

    return true;
}

bool dht_read_float_data(dht_sensor_type_t sensor_type, uint8_t pin, float *humidity, float *temperature)
{
    if (!dht_lock) {
        dht_lock = xSemaphoreCreateMutex();
    }
    
    int16_t i_humidity, i_temp;

    if (dht_read_data(sensor_type, pin, &i_humidity, &i_temp)) {
        *humidity = (float)i_humidity / 10;
        *temperature = (float)i_temp / 10;
        return true;
    }
    return false;
}
