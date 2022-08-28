/*
 * Driver for Sensirion SHT3x digital temperature and humidity sensor
 * connected to I2C
 *
 * This driver is for the usage with the ESP8266 and FreeRTOS (esp-open-rtos)
 * [https://github.com/SuperHouse/esp-open-rtos]. It is also working with ESP32
 * and ESP-IDF [https://github.com/espressif/esp-idf.git] as well as Linux
 * based systems using a wrapper library for ESP8266 functions.
 *
 * ----------------------------------------------------------------
 *
 * The BSD License (3-clause license)
 *
 * Copyright (c) 2017 Gunar Schorcht (https://github.com/gschorcht)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its 
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <string.h>
#include <stdlib.h>

#include "sht3x.h"

#define SHT3x_STATUS_CMD               0xF32D
#define SHT3x_CLEAR_STATUS_CMD         0x3041
#define SHT3x_RESET_CMD                0x30A2
#define SHT3x_FETCH_DATA_CMD           0xE000
#define SHT3x_HEATER_OFF_CMD           0x3066

const uint16_t SHT3x_MEASURE_CMD[6][3] = { 
        {0x2400,0x240b,0x2416},    // [SINGLE_SHOT][H,M,L] without clock stretching
        {0x2032,0x2024,0x202f},    // [PERIODIC_05][H,M,L]
        {0x2130,0x2126,0x212d},    // [PERIODIC_1 ][H,M,L]
        {0x2236,0x2220,0x222b},    // [PERIODIC_2 ][H,M,L]
        {0x2234,0x2322,0x2329},    // [PERIODIC_4 ][H,M,L]
        {0x2737,0x2721,0x272a} };  // [PERIODIC_10][H,M,L]

// due to the fact that ticks can be smaller than portTICK_PERIOD_MS, one and
// a half tick period added to the duration to be sure that waiting time for 
// the results is long enough
#define TIME_TO_TICKS(ms) (1 + ((ms) + (portTICK_PERIOD_MS-1) + portTICK_PERIOD_MS/2 ) / portTICK_PERIOD_MS)

#define SHT3x_MEAS_DURATION_REP_HIGH   15
#define SHT3x_MEAS_DURATION_REP_MEDIUM 6
#define SHT3x_MEAS_DURATION_REP_LOW    4

// measurement durations in us 
const uint16_t SHT3x_MEAS_DURATION_US[3] = { SHT3x_MEAS_DURATION_REP_HIGH   * 1000, 
                                             SHT3x_MEAS_DURATION_REP_MEDIUM * 1000, 
                                             SHT3x_MEAS_DURATION_REP_LOW    * 1000 };

// measurement durations in RTOS ticks
const uint8_t SHT3x_MEAS_DURATION_TICKS[3] = { TIME_TO_TICKS(SHT3x_MEAS_DURATION_REP_HIGH), 
                                               TIME_TO_TICKS(SHT3x_MEAS_DURATION_REP_MEDIUM), 
                                               TIME_TO_TICKS(SHT3x_MEAS_DURATION_REP_LOW) };

#if defined(SHT3x_DEBUG_LEVEL_2)
#define debug(s, f, ...) printf("%s %s: " s "\n", "SHT3x", f, ## __VA_ARGS__)
#define debug_dev(s, f, d, ...) printf("%s %s: bus %d, addr %02x - " s "\n", "SHT3x", f, d->bus, d->addr, ## __VA_ARGS__)
#else
#define debug(s, f, ...)
#define debug_dev(s, f, d, ...)
#endif

#if defined(SHT3x_DEBUG_LEVEL_1) || defined(SHT3x_DEBUG_LEVEL_2)
#define error(s, f, ...) printf("%s %s: " s "\n", "SHT3x", f, ## __VA_ARGS__)
#define error_dev(s, f, d, ...) printf("%s %s: bus %d, addr %02x - " s "\n", "SHT3x", f, d->bus, d->addr, ## __VA_ARGS__)
#else
#define error(s, f, ...)
#define error_dev(s, f, d, ...)
#endif

/** Forward declaration of function for internal use */

static bool sht3x_is_measuring  (sht3x_sensor_t*);
static bool sht3x_send_command  (sht3x_sensor_t*, uint16_t);
static bool sht3x_read_data     (sht3x_sensor_t*, uint8_t*,  uint32_t);
static bool sht3x_get_status    (sht3x_sensor_t*, uint16_t*);
static bool sht3x_reset         (sht3x_sensor_t*);

static uint8_t crc8 (uint8_t data[], int len);

/** ------------------------------------------------ */

bool sht3x_init_driver()
{
    return true;
}


sht3x_sensor_t* sht3x_init_sensor(uint8_t bus, uint8_t addr)
{
    sht3x_sensor_t* dev;

    if ((dev = malloc (sizeof(sht3x_sensor_t))) == NULL)
        return NULL;
    
    // inititalize sensor data structure
    dev->bus  = bus;
    dev->addr = addr;
    dev->mode = sht3x_single_shot;
    dev->meas_start_time = 0;
    dev->meas_started = false;
    dev->meas_first = false;

    uint16_t status;

    // try to reset the sensor
    if (!sht3x_reset(dev))
    {
        debug_dev ("could not reset the sensor", __FUNCTION__, dev);
    }
    
    // check again the status after clear status command
    if (!sht3x_get_status(dev, &status))
    {
        error_dev ("could not get sensor status", __FUNCTION__, dev);
        free(dev);
        return NULL;       
    }
    
    debug_dev ("sensor initialized", __FUNCTION__, dev);
    return dev;
}


bool sht3x_measure (sht3x_sensor_t* dev, float* temperature, float* humidity)
{
    if (!dev || (!temperature && !humidity)) return false;

    if (!sht3x_start_measurement (dev, sht3x_single_shot, sht3x_high))
        return false;

    vTaskDelay (SHT3x_MEAS_DURATION_TICKS[sht3x_high]);

    sht3x_raw_data_t raw_data;
    
    if (!sht3x_get_raw_data (dev, raw_data))
        return false;
        
    return sht3x_compute_values (raw_data, temperature, humidity);
}


bool sht3x_start_measurement (sht3x_sensor_t* dev, sht3x_mode_t mode, sht3x_repeat_t repeat)
{
    if (!dev) return false;
    
    dev->error_code = SHT3x_OK;
    dev->mode = mode;
    dev->repeatability = repeat;
    
    // start measurement according to selected mode and return an duration estimate
    if (!sht3x_send_command(dev, SHT3x_MEASURE_CMD[mode][repeat]))
    {
        error_dev ("could not send start measurment command", __FUNCTION__, dev);
        dev->error_code |= SHT3x_SEND_MEAS_CMD_FAILED;
        return false;
    }

    dev->meas_start_time = sdk_system_get_time ();
    dev->meas_started = true;
    dev->meas_first = true;

    return true;
}


uint8_t sht3x_get_measurement_duration (sht3x_repeat_t repeat)
{
    return SHT3x_MEAS_DURATION_TICKS[repeat];  // in RTOS ticks
}


bool sht3x_get_raw_data(sht3x_sensor_t* dev, sht3x_raw_data_t raw_data)
{
    if (!dev || !raw_data) return false;

    dev->error_code = SHT3x_OK;

    if (!dev->meas_started)
    {
        debug_dev ("measurement is not started", __FUNCTION__, dev);
        dev->error_code = SHT3x_MEAS_NOT_STARTED;
        return sht3x_is_measuring (dev);
    }

    if (sht3x_is_measuring(dev))
    {
        error_dev ("measurement is still running", __FUNCTION__, dev);
        dev->error_code = SHT3x_MEAS_STILL_RUNNING;
        return false;
    }

    // send fetch command in any periodic mode (mode > 0) before read raw data
    if (dev->mode && !sht3x_send_command(dev, SHT3x_FETCH_DATA_CMD))
    {
        debug_dev ("send fetch command failed", __FUNCTION__, dev);
        dev->error_code |= SHT3x_SEND_FETCH_CMD_FAILED;
        return false;
    }

    // read raw data
    if (!sht3x_read_data(dev, raw_data, sizeof(sht3x_raw_data_t)))
    {
        error_dev ("read raw data failed", __FUNCTION__, dev);
        dev->error_code |= SHT3x_READ_RAW_DATA_FAILED;
        return false;
    }

    // reset first measurement flag
    dev->meas_first = false;
    
    // reset measurement started flag in single shot mode
    if (dev->mode == sht3x_single_shot)
        dev->meas_started = false;
    
    // check temperature crc
    if (crc8(raw_data,2) != raw_data[2])
    {
        error_dev ("CRC check for temperature data failed", __FUNCTION__, dev);
        dev->error_code |= SHT3x_WRONG_CRC_TEMPERATURE;
        return false;
    }

    // check humidity crc
    if (crc8(raw_data+3,2) != raw_data[5])
    {
        error_dev ("CRC check for humidity data failed", __FUNCTION__, dev);
        dev->error_code |= SHT3x_WRONG_CRC_HUMIDITY;
        return false;
    }

    return true;
}


bool sht3x_compute_values (sht3x_raw_data_t raw_data, float* temperature, float* humidity)
{
    if (!raw_data) return false;

    if (temperature) 
        *temperature = ((((raw_data[0] * 256.0) + raw_data[1]) * 175) / 65535.0) - 45;

    if (humidity)
        *humidity = ((((raw_data[3] * 256.0) + raw_data[4]) * 100) / 65535.0);
  
    return true;    
}


bool sht3x_get_results (sht3x_sensor_t* dev, float* temperature, float* humidity)
{
    if (!dev || (!temperature && !humidity)) return false;

    sht3x_raw_data_t raw_data;
    
    if (!sht3x_get_raw_data (dev, raw_data))
        return false;
        
    return sht3x_compute_values (raw_data, temperature, humidity);
}

/* Functions for internal use only */

static bool sht3x_is_measuring (sht3x_sensor_t* dev)
{
    if (!dev) return false;

    dev->error_code = SHT3x_OK;

    // not running if measurement is not started at all or 
    // it is not the first measurement in periodic mode
    if (!dev->meas_started || !dev->meas_first)
      return false;
    
    // not running if time elapsed is greater than duration
    uint32_t elapsed = sdk_system_get_time() - dev->meas_start_time;

    return elapsed < SHT3x_MEAS_DURATION_US[dev->repeatability];
}


static bool sht3x_send_command(sht3x_sensor_t* dev, uint16_t cmd)
{
    if (!dev) return false;

    uint8_t data[2] = { cmd >> 8, cmd & 0xff };

    debug_dev ("send command MSB=%02x LSB=%02x", __FUNCTION__, dev, data[0], data[1]);

    int err = i2c_slave_write(dev->bus, dev->addr, 0, data, 2);
  
    if (err)
    {
        dev->error_code |= (err == -EBUSY) ? SHT3x_I2C_BUSY : SHT3x_I2C_SEND_CMD_FAILED;
        error_dev ("i2c error %d on write command %02x", __FUNCTION__, dev, err, cmd);
        return false;
    }

    return true;
}

static bool sht3x_read_data(sht3x_sensor_t* dev, uint8_t *data,  uint32_t len)
{
    if (!dev) return false;
    int err = i2c_slave_read(dev->bus, dev->addr, 0, data, len);
        
    if (err)
    {
        dev->error_code |= (err == -EBUSY) ? SHT3x_I2C_BUSY : SHT3x_I2C_READ_FAILED;
        error_dev ("error %d on read %d byte", __FUNCTION__, dev, err, len);
        return false;
    }

#   ifdef SHT3x_DEBUG_LEVEL_2
    printf("SHT3x %s: bus %d, addr %02x - read following bytes: ", 
           __FUNCTION__, dev->bus, dev->addr);
    for (int i=0; i < len; i++)
        printf("%02x ", data[i]);
    printf("\n");
#   endif // ifdef SHT3x_DEBUG_LEVEL_2

    return true;
}


static bool sht3x_reset (sht3x_sensor_t* dev)
{
    if (!dev) return false;

    debug_dev ("soft-reset triggered", __FUNCTION__, dev);
    
    dev->error_code = SHT3x_OK;

    // send reset command
    if (!sht3x_send_command(dev, SHT3x_RESET_CMD))
    {
        dev->error_code |= SHT3x_SEND_RESET_CMD_FAILED;
        return false;
    }   
    // wait for small amount of time needed (according to datasheet 0.5ms)
    vTaskDelay (100 / portTICK_PERIOD_MS);
    
    uint16_t status;

    // check the status after reset
    if (!sht3x_get_status(dev, &status))
        return false;
        
    return true;    
}


static bool sht3x_get_status (sht3x_sensor_t* dev, uint16_t* status)
{
    if (!dev || !status) return false;

    dev->error_code = SHT3x_OK;

    uint8_t  data[3];

    if (!sht3x_send_command(dev, SHT3x_STATUS_CMD) || !sht3x_read_data(dev, data, 3))
    {
        dev->error_code |= SHT3x_SEND_STATUS_CMD_FAILED;
        return false;
    }

    *status = data[0] << 8 | data[1];
    debug_dev ("status=%02x", __FUNCTION__, dev, *status);
    return true;
}


const uint8_t g_polynom = 0x31;

static uint8_t crc8 (uint8_t data[], int len)
{
    // initialization value
    uint8_t crc = 0xff;
    
    // iterate over all bytes
    for (int i=0; i < len; i++)
    {
        crc ^= data[i];  
    
        for (int i = 0; i < 8; i++)
        {
            bool xor = crc & 0x80;
            crc = crc << 1;
            crc = xor ? crc ^ g_polynom : crc;
        }
    }

    return crc;
} 


