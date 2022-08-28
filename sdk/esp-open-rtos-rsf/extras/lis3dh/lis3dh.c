/*
 * Driver for LIS3DH 3-axes digital accelerometer connected to I2C or SPI.
 *
 * This driver is for the usage with the ESP8266 and FreeRTOS (esp-open-rtos)
 * [https://github.com/SuperHouse/esp-open-rtos]. It is also working with ESP32
 * and ESP-IDF [https://github.com/espressif/esp-idf.git] as well as Linux
 * based systems using a wrapper library for ESP8266 functions.
 *
 * ---------------------------------------------------------------------------
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
 *
 * The information provided is believed to be accurate and reliable. The
 * copyright holder assumes no responsibility for the consequences of use
 * of such information nor for any infringement of patents or other rights
 * of third parties which may result from its use. No license is granted by
 * implication or otherwise under any patent or patent rights of the copyright
 * holder.
 */

#include <string.h>
#include <stdlib.h>

#include "lis3dh.h"

#if defined(LIS3DH_DEBUG_LEVEL_2)
#define debug(s, f, ...) printf("%s %s: " s "\n", "LIS3DH", f, ## __VA_ARGS__)
#define debug_dev(s, f, d, ...) printf("%s %s: bus %d, addr %02x - " s "\n", "LIS3DH", f, d->bus, d->addr, ## __VA_ARGS__)
#else
#define debug(s, f, ...)
#define debug_dev(s, f, d, ...)
#endif

#if defined(LIS3DH_DEBUG_LEVEL_1) || defined(LIS3DH_DEBUG_LEVEL_2)
#define error(s, f, ...) printf("%s %s: " s "\n", "LIS3DH", f, ## __VA_ARGS__)
#define error_dev(s, f, d, ...) printf("%s %s: bus %d, addr %02x - " s "\n", "LIS3DH", f, d->bus, d->addr, ## __VA_ARGS__)
#else
#define error(s, f, ...)
#define error_dev(s, f, d, ...)
#endif

// register addresses
#define LIS3DH_REG_STATUS_AUX    0x07
#define LIS3DH_REG_OUT_ADC1_L    0x08
#define LIS3DH_REG_OUT_ADC1_H    0x09
#define LIS3DH_REG_OUT_ADC2_L    0x0a
#define LIS3DH_REG_OUT_ADC2_H    0x0b
#define LIS3DH_REG_OUT_ADC3_L    0x0c
#define LIS3DH_REG_OUT_ADC3_H    0x0d
#define LIS3DH_REG_INT_COUNTER   0x0e
#define LIS3DH_REG_WHO_AM_I      0x0f
#define LIS3DH_REG_TEMP_CFG      0x1f
#define LIS3DH_REG_CTRL1         0x20
#define LIS3DH_REG_CTRL2         0x21
#define LIS3DH_REG_CTRL3         0x22
#define LIS3DH_REG_CTRL4         0x23
#define LIS3DH_REG_CTRL5         0x24
#define LIS3DH_REG_CTRL6         0x25
#define LIS3DH_REG_REFERENCE     0x26
#define LIS3DH_REG_STATUS        0x27
#define LIS3DH_REG_OUT_X_L       0x28
#define LIS3DH_REG_OUT_X_H       0x29
#define LIS3DH_REG_OUT_Y_L       0x2a
#define LIS3DH_REG_OUT_Y_H       0x2b
#define LIS3DH_REG_OUT_Z_L       0x2c
#define LIS3DH_REG_OUT_Z_H       0x2d
#define LIS3DH_REG_FIFO_CTRL     0x2e
#define LIS3DH_REG_FIFO_SRC      0x2f
#define LIS3DH_REG_INT1_CFG      0x30
#define LIS3DH_REG_INT1_SRC      0x31
#define LIS3DH_REG_INT1_THS      0x32
#define LIS3DH_REG_INT1_DUR      0x33
#define LIS3DH_REG_INT2_CFG      0x34
#define LIS3DH_REG_INT2_SRC      0x35
#define LIS3DH_REG_INT2_THS      0x36
#define LIS3DH_REG_INT2_DUR      0x37
#define LIS3DH_REG_CLICK_CFG     0x38
#define LIS3DH_REG_CLICK_SRC     0x39
#define LIS3DH_REG_CLICK_THS     0x3a
#define LIS3DH_REG_TIME_LIMIT    0x3b
#define LIS3DH_REG_TIME_LATENCY  0x3c
#define LIS3DH_REG_TIME_WINDOW   0x3d

// register structure definitions
struct lis3dh_reg_status 
{
    uint8_t XDA   :1;      // STATUS<0>   X axis new data available
    uint8_t YDA   :1;      // STATUS<1>   Y axis new data available
    uint8_t ZDA   :1;      // STATUS<2>   Z axis new data available
    uint8_t ZYXDA :1;      // STATUS<3>   X, Y and Z axis new data available
    uint8_t XOR   :1;      // STATUS<4>   X axis data overrun
    uint8_t YOR   :1;      // STATUS<5>   Y axis data overrun 
    uint8_t ZOR   :1;      // STATUS<6>   Z axis data overrun
    uint8_t ZYXOR :1;      // STATUS<7>   X, Y and Z axis data overrun
};

#define LIS3DH_ANY_DATA_READY    0x0f    // LIS3DH_REG_STATUS<3:0>

struct lis3dh_reg_ctrl1 
{
    uint8_t Xen  :1;       // CTRL1<0>    X axis enable
    uint8_t Yen  :1;       // CTRL1<1>    Y axis enable
    uint8_t Zen  :1;       // CTRL1<2>    Z axis enable
    uint8_t LPen :1;       // CTRL1<3>    Low power mode enable
    uint8_t ODR  :4;       // CTRL1<7:4>  Data rate selection
};

struct lis3dh_reg_ctrl2 
{
    uint8_t HPIS1   :1;    // CTRL2<0>    HPF enabled for AOI on INT2
    uint8_t HPIS2   :1;    // CTRL2<1>    HPF enabled for AOI on INT2
    uint8_t HPCLICK :1;    // CTRL2<2>    HPF enabled for CLICK
    uint8_t FDS     :1;    // CTRL2<3>    Filter data selection
    uint8_t HPCF    :2;    // CTRL2<5:4>  HPF cutoff frequency
    uint8_t HPM     :2;    // CTRL2<7:6>  HPF mode
};

struct lis3dh_reg_ctrl3 
{
    uint8_t unused     :1; // CTRL3<0>  unused
    uint8_t I1_OVERRUN :1; // CTRL3<1>  FIFO Overrun interrupt on INT1
    uint8_t I1_WTM1    :1; // CTRL3<2>  FIFO Watermark interrupt on INT1
    uint8_t IT_DRDY2   :1; // CTRL3<3>  DRDY2 (ZYXDA) interrupt on INT1
    uint8_t IT_DRDY1   :1; // CTRL3<4>  DRDY1 (321DA) interrupt on INT1
    uint8_t I1_AOI2    :1; // CTRL3<5>  AOI2 interrupt on INT1
    uint8_t I1_AOI1    :1; // CTRL3<6>  AOI1 interrupt on INT1
    uint8_t I1_CLICK   :1; // CTRL3<7>  CLICK interrupt on INT1
};

struct lis3dh_reg_ctrl4 
{
    uint8_t SIM :1;        // CTRL4<0>   SPI serial interface selection
    uint8_t ST  :2;        // CTRL4<2:1> Self test enable
    uint8_t HR  :1;        // CTRL4<3>   High resolution output mode
    uint8_t FS  :2;        // CTRL4<5:4> Full scale selection
    uint8_t BLE :1;        // CTRL4<6>   Big/litle endian data selection
    uint8_t BDU :1;        // CTRL4<7>   Block data update
};

struct lis3dh_reg_ctrl5 
{
    uint8_t D4D_INT2 :1;   // CTRL5<0>   4D detection enabled on INT1
    uint8_t LIR_INT2 :1;   // CTRL5<1>   Latch interrupt request on INT1
    uint8_t D4D_INT1 :1;   // CTRL5<2>   4D detection enabled on INT2
    uint8_t LIR_INT1 :1;   // CTRL5<3>   Latch interrupt request on INT1
    uint8_t unused   :2;   // CTRL5<5:4> unused
    uint8_t FIFO_EN  :1;   // CTRL5<6>   FIFO enabled
    uint8_t BOOT     :1;   // CTRL5<7>   Reboot memory content
};

struct lis3dh_reg_ctrl6 
{
    uint8_t unused1  :1;   // CTRL6<0>   unused
    uint8_t H_LACTIVE:1;   // CTRL6<1>   Interrupt polarity
    uint8_t unused2  :1;   // CTRL6<2>   unused
    uint8_t I2_ACT   :1;   // CTRL6<3>   ?
    uint8_t I2_BOOT  :1;   // CTRL6<4>   ?
    uint8_t I2_AOI2  :1;   // CTRL6<5>   AOI2 interrupt on INT1
    uint8_t I2_AOI1  :1;   // CTRL6<6>   AOI1 interrupt on INT1
    uint8_t I2_CLICK :1;   // CTRL6<7>   CLICK interrupt on INT2
};

struct lis3dh_reg_fifo_ctrl
{
    uint8_t FTH :5;        // FIFO_CTRL<4:0>  FIFO threshold
    uint8_t TR  :1;        // FIFO_CTRL<5>    Trigger selection INT1 / INT2
    uint8_t FM  :2;        // FIFO_CTRL<7:6>  FIFO mode
};

struct lis3dh_reg_fifo_src
{
    uint8_t FFS       :5;  // FIFO_SRC<4:0>  FIFO samples stored
    uint8_t EMPTY     :1;  // FIFO_SRC<5>    FIFO is empty
    uint8_t OVRN_FIFO :1;  // FIFO_SRC<6>    FIFO buffer full
    uint8_t WTM       :1;  // FIFO_SRC<7>    FIFO content exceeds watermark
};

struct lis3dh_reg_intx_cfg
{
    uint8_t XLIE :1;   // INTx_CFG<0>    X axis below threshold enabled
    uint8_t XHIE :1;   // INTx_CFG<1>    X axis above threshold enabled
    uint8_t YLIE :1;   // INTx_CFG<2>    Y axis below threshold enabled
    uint8_t YHIE :1;   // INTx_CFG<3>    Y axis above threshold enabled
    uint8_t ZLIE :1;   // INTx_CFG<4>    Z axis below threshold enabled
    uint8_t ZHIE :1;   // INTx_CFG<5>    Z axis above threshold enabled
    uint8_t SIXD :1;   // INTx_CFG<6>    6D/4D orientation detecetion enabled
    uint8_t AOI  :1;   // INTx_CFG<7>    AND/OR combination of interrupt events
};

struct lis3dh_reg_intx_src
{
    uint8_t XL    :1;  // INTx_SRC<0>    X axis below threshold enabled
    uint8_t XH    :1;  // INTx_SRC<1>    X axis above threshold enabled
    uint8_t YL    :1;  // INTx_SRC<2>    Y axis below threshold enabled
    uint8_t YH    :1;  // INTx_SRC<3>    Y axis above threshold enabled
    uint8_t ZL    :1;  // INTx_SRC<4>    Z axis below threshold enabled
    uint8_t ZH    :1;  // INTx_SRC<5>    Z axis above threshold enabled
    uint8_t IA    :1;  // INTx_SRC<6>    Interrupt active
    uint8_t unused:1;  // INTx_SRC<7>    unused
};


struct lis3dh_reg_click_cfg
{
    uint8_t XS    :1;  // CLICK_CFG<0>    X axis single click enabled
    uint8_t XD    :1;  // CLICK_CFG<1>    X axis double click enabled
    uint8_t YS    :1;  // CLICK_CFG<2>    Y axis single click enabled
    uint8_t YD    :1;  // CLICK_CFG<3>    Y axis double click enabled
    uint8_t ZS    :1;  // CLICK_CFG<4>    Z axis single click enabled
    uint8_t ZD    :1;  // CLICK_CFG<5>    Z axis double click enabled
    uint8_t unused:2;  // CLICK_CFG<7:6>  unused
};


/** Forward declaration of functions for internal use */

static bool    lis3dh_reset       (lis3dh_sensor_t* dev);
static bool    lis3dh_is_available(lis3dh_sensor_t* dev);

static bool    lis3dh_i2c_read    (lis3dh_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len);
static bool    lis3dh_i2c_write   (lis3dh_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len);
static bool    lis3dh_spi_read    (lis3dh_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len);
static bool    lis3dh_spi_write   (lis3dh_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len);

#define msb_lsb_to_type(t,b,o) (t)(((t)b[o] << 8) | b[o+1])
#define lsb_msb_to_type(t,b,o) (t)(((t)b[o+1] << 8) | b[o])
#define lsb_to_type(t,b,o)     (t)(b[o])

#define lis3dh_update_reg(dev,addr,type,elem,value) \
        { \
            struct type __reg; \
            if (!lis3dh_reg_read (dev, (addr), (uint8_t*)&__reg, 1)) \
                return false; \
            __reg.elem = (value); \
            if (!lis3dh_reg_write (dev, (addr), (uint8_t*)&__reg, 1)) \
                return false; \
        }

lis3dh_sensor_t* lis3dh_init_sensor (uint8_t bus, uint8_t addr, uint8_t cs)
{
    lis3dh_sensor_t* dev;

    if ((dev = malloc (sizeof(lis3dh_sensor_t))) == NULL)
        return NULL;

    // init sensor data structure
    dev->bus    = bus;
    dev->addr   = addr;
    dev->cs     = cs;

    dev->error_code = LIS3DH_OK;
    dev->scale      = lis3dh_scale_2_g;
    dev->fifo_mode  = lis3dh_bypass;
    dev->fifo_first = true;
    
    // if addr==0 then SPI is used and has to be initialized
    if (!addr && !spi_device_init (bus, cs))
    {
        error_dev ("Could not initialize SPI interface.", __FUNCTION__, dev);
        free (dev);
        return NULL;
    }
        
    // check availability of the sensor
    if (!lis3dh_is_available (dev))
    {
        error_dev ("Sensor is not available.", __FUNCTION__, dev);
        free (dev);
        return NULL;
    }

    // reset the sensor
    if (!lis3dh_reset(dev))
    {
        error_dev ("Could not reset the sensor device.", __FUNCTION__, dev);
        free (dev);
        return NULL;
    }
    
    lis3dh_update_reg (dev, LIS3DH_REG_CTRL4, lis3dh_reg_ctrl4, FS, lis3dh_scale_2_g);
    lis3dh_update_reg (dev, LIS3DH_REG_CTRL4, lis3dh_reg_ctrl4, BDU, 1);

    return dev;
}

bool lis3dh_set_mode (lis3dh_sensor_t* dev, 
                      lis3dh_odr_mode_t odr, lis3dh_resolution_t res,
                      bool x, bool y, bool z)
{
    if (!dev) return false;

    dev->error_code = LIS3DH_OK;
    dev->res = res;

    struct lis3dh_reg_ctrl1 reg;
    uint8_t old_odr;

    // read current register values
    if (!lis3dh_reg_read (dev, LIS3DH_REG_CTRL1, (uint8_t*)&reg, 1))
        return false;
   
    old_odr = reg.ODR;
    
    // set mode
    reg.Xen  = x;
    reg.Yen  = y;
    reg.Zen  = z;
    reg.ODR  = odr;
    reg.LPen = (res == lis3dh_low_power);

    lis3dh_update_reg (dev, LIS3DH_REG_CTRL4, lis3dh_reg_ctrl4, 
                       HR, (res == lis3dh_high_res));
    
    if (!lis3dh_reg_write (dev, LIS3DH_REG_CTRL1, (uint8_t*)&reg, 1))
        return false;
    
    // if sensor was in power down mode it takes at least 100 ms to start in another mode
    if (old_odr == lis3dh_power_down && odr != lis3dh_power_down)
        vTaskDelay (15);

    return false;
}


bool lis3dh_set_scale (lis3dh_sensor_t* dev, lis3dh_scale_t scale)
{
    if (!dev) return false;
    
    dev->error_code = LIS3DH_OK;
    dev->scale = scale;
    
    // read CTRL4 register and write scale
    lis3dh_update_reg (dev, LIS3DH_REG_CTRL4, lis3dh_reg_ctrl4, FS, scale);
    
    return true;
}


bool lis3dh_set_fifo_mode (lis3dh_sensor_t* dev, lis3dh_fifo_mode_t mode,
                           uint8_t thresh, lis3dh_int_signal_t trigger)
{
    if (!dev) return false;
    
    dev->error_code = LIS3DH_OK;
    dev->fifo_mode = mode;
    
    // read CTRL5 register and write FIFO_EN flag
    lis3dh_update_reg (dev, LIS3DH_REG_CTRL5, lis3dh_reg_ctrl5, FIFO_EN, mode != lis3dh_bypass);

    struct lis3dh_reg_fifo_ctrl fifo_ctrl = {
        .FTH = thresh,
        .TR  = trigger,
        .FM  = mode,
    };

    // write FIFO_CTRL register
    if (!lis3dh_reg_write (dev, LIS3DH_REG_FIFO_CTRL, (uint8_t*)&fifo_ctrl, 1))
        return false;

    return true;
}


bool lis3dh_new_data (lis3dh_sensor_t* dev)
{
    if (!dev) return false;

    dev->error_code = LIS3DH_OK;

    if (dev->fifo_mode == lis3dh_bypass)
    {
        struct lis3dh_reg_status status;
        
        if (!lis3dh_reg_read (dev, LIS3DH_REG_STATUS, (uint8_t*)&status, 1))
        {
            error_dev ("Could not get sensor status", __FUNCTION__, dev);
            return false;
        }
        return status.ZYXDA;
    }
    else
    {
        struct lis3dh_reg_fifo_src fifo_src;
        
        if (!lis3dh_reg_read (dev, LIS3DH_REG_FIFO_SRC, (uint8_t*)&fifo_src, 1))
        {
            error_dev ("Could not get fifo source register data", __FUNCTION__, dev);
            return false;
        }
        return !fifo_src.EMPTY;
    }
}

/**
 * Scaling factors for the conversion of raw sensor data to floating point g
 * values. Scaling factors are from mechanical characteristics in datasheet.
 *
 *  scale/sensitivity  resolution
 *       +-1g           1 mg/digit
 *       +-2g           2 mg/digit
 *       +-4g           4 mg/digit
 *      +-16g          12 mg/digit
 */
const static double  LIS3DH_SCALES[4] = { 0.001, 0.002, 0.004, 0.012 };

bool lis3dh_get_float_data (lis3dh_sensor_t* dev, lis3dh_float_data_t* data)
{
    if (!dev || !data) return false;

    lis3dh_raw_data_t raw;
    
    if (!lis3dh_get_raw_data (dev, &raw))
        return false;

    data->ax = LIS3DH_SCALES[dev->scale] * (raw.ax >> 4);
    data->ay = LIS3DH_SCALES[dev->scale] * (raw.ay >> 4);
    data->az = LIS3DH_SCALES[dev->scale] * (raw.az >> 4);

    return true;
}


uint8_t lis3dh_get_float_data_fifo (lis3dh_sensor_t* dev, lis3dh_float_data_fifo_t data)
{
    if (!dev || !data) return false;

    lis3dh_raw_data_fifo_t raw;
    
    uint8_t num = lis3dh_get_raw_data_fifo (dev, raw);

    for (int i = 0; i < num; i++)
    {
        data[i].ax = LIS3DH_SCALES[dev->scale] * (raw[i].ax >> 4);
        data[i].ay = LIS3DH_SCALES[dev->scale] * (raw[i].ay >> 4);
        data[i].az = LIS3DH_SCALES[dev->scale] * (raw[i].az >> 4);
    }
    return num;
}


bool lis3dh_get_raw_data (lis3dh_sensor_t* dev, lis3dh_raw_data_t* raw)
{
    if (!dev || !raw) return false;

    dev->error_code = LIS3DH_OK;

    // abort if not in bypass mode
    if (dev->fifo_mode != lis3dh_bypass)
    {
        dev->error_code = LIS3DH_SENSOR_IN_BYPASS_MODE;
        error_dev ("Sensor is in FIFO mode, use lis3dh_get_*_data_fifo to get data",
                   __FUNCTION__, dev);
        return false;
    }

    // read raw data sample
    if (!lis3dh_reg_read (dev, LIS3DH_REG_OUT_X_L, (uint8_t*)raw, 6))
    {
        error_dev ("Could not get raw data sample", __FUNCTION__, dev);
        dev->error_code |= LIS3DH_GET_RAW_DATA_FAILED;
        return false;
    }

    return true;
}


uint8_t lis3dh_get_raw_data_fifo (lis3dh_sensor_t* dev, lis3dh_raw_data_fifo_t raw)
{
    if (!dev) return 0;

    dev->error_code = LIS3DH_OK;

    // in bypass mode, use lis3dh_get_raw_data to return one sample
    if (dev->fifo_mode == lis3dh_bypass)
        return lis3dh_get_raw_data (dev, raw) ? 1 : 0;
        
    struct lis3dh_reg_fifo_src fifo_src;
    
    // read FIFO state
    if (!lis3dh_reg_read (dev, LIS3DH_REG_FIFO_SRC, (uint8_t*)&fifo_src, 1))
    {
        error_dev ("Could not get fifo source register data", __FUNCTION__, dev);
        return 0;
    }

    // if nothing is in the FIFO, just return with 0
    if (fifo_src.EMPTY)
        return 0;

    uint8_t samples = fifo_src.FFS + (fifo_src.OVRN_FIFO ? 1 : 0);

    // read samples from FIFO
    for (int i = 0; i < samples; i++)
        if (!lis3dh_reg_read (dev, LIS3DH_REG_OUT_X_L, (uint8_t*)&raw[i], 6))
        {
            error_dev ("Could not get raw data samples", __FUNCTION__, dev);
            dev->error_code |= LIS3DH_GET_RAW_DATA_FIFO_FAILED;
            return i;
        }

    lis3dh_reg_read (dev, LIS3DH_REG_FIFO_SRC, (uint8_t*)&fifo_src, 1);
    
    // if FFS is not 0 after all samples read, ODR is higher than fetching rate
    if (fifo_src.FFS)
    {
        dev->error_code = LIS3DH_ODR_TOO_HIGH;
        error_dev ("New samples stored in FIFO while reading, "
                   "output data rate (ODR) too high", __FUNCTION__, dev);
        return 0;
    }

    if (dev->fifo_mode == lis3dh_fifo && samples == 32)
    {
        // clean FIFO (see app note)
        lis3dh_update_reg (dev, LIS3DH_REG_FIFO_CTRL, lis3dh_reg_fifo_ctrl, FM, lis3dh_bypass);
        lis3dh_update_reg (dev, LIS3DH_REG_FIFO_CTRL, lis3dh_reg_fifo_ctrl, FM, lis3dh_fifo);
    }

    return samples;
}


bool lis3dh_enable_int (lis3dh_sensor_t* dev, 
                        lis3dh_int_type_t type, 
                        lis3dh_int_signal_t signal, bool value)
{
    if (!dev) return false;

    dev->error_code = LIS3DH_OK;

    struct lis3dh_reg_ctrl3  ctrl3;
    struct lis3dh_reg_ctrl6  ctrl6;

    uint8_t* reg = NULL;
    uint8_t  addr;

    // determine the addr of the register to change
    if (type == lis3dh_int_data_ready     ||
        type == lis3dh_int_fifo_watermark ||
        type == lis3dh_int_fifo_overrun)
    {
        reg  = (uint8_t*)&ctrl3;
        addr = LIS3DH_REG_CTRL3;
    }
    else if (signal == lis3dh_int1_signal)
    {
        reg  = (uint8_t*)&ctrl3;
        addr = LIS3DH_REG_CTRL3;
    }
    else
    {
        reg  = (uint8_t*)&ctrl6;
        addr = LIS3DH_REG_CTRL6;
    }

    // read the register
    if (!lis3dh_reg_read (dev, addr, reg, 1))
    {   
        error_dev ("Could not read interrupt control registers", __FUNCTION__, dev);
        dev->error_code |= LIS3DH_CONFIG_INT_FAILED;
        return false;
    }

    // change the register
    switch (type)
    {
        case lis3dh_int_data_ready:     ctrl3.IT_DRDY1 = value;
                                        break;
                                        
        case lis3dh_int_fifo_watermark: ctrl3.I1_WTM1 = value;
                                        break;
                                        
        case lis3dh_int_fifo_overrun:   ctrl3.I1_OVERRUN = value;
                                        break;
                                        
        case lis3dh_int_event1:         if (signal == lis3dh_int1_signal)
                                            ctrl3.I1_AOI1 = value;
                                        else
                                            ctrl6.I2_AOI1 = value;
                                        break;

        case lis3dh_int_event2:         if (signal == lis3dh_int1_signal)
                                            ctrl3.I1_AOI2 = value;
                                        else
                                            ctrl6.I2_AOI2 = value;
                                        break;

        case lis3dh_int_click:          if (signal == lis3dh_int1_signal)
                                            ctrl3.I1_CLICK = value;
                                        else
                                            ctrl6.I2_CLICK = value;
                                        break;
                      
        default: dev->error_code = LIS3DH_WRONG_INT_TYPE; 
                 error_dev ("Wrong interrupt type", __FUNCTION__, dev);
                 return false;
    }        
    
    if (!lis3dh_reg_write (dev, addr, reg, 1))
    {   
        error_dev ("Could not enable/disable interrupt", __FUNCTION__, dev);
        dev->error_code |= LIS3DH_CONFIG_INT_FAILED;
        return false;
    }
    
    return true;
}


bool lis3dh_get_int_data_source (lis3dh_sensor_t* dev, 
                                 lis3dh_int_data_source_t* source)
{
    if (!dev || !source) return false;

    dev->error_code = LIS3DH_OK;

    struct lis3dh_reg_ctrl3    ctrl3;
    struct lis3dh_reg_status   status;
    struct lis3dh_reg_fifo_src fifo_src;

    if (!lis3dh_reg_read (dev, LIS3DH_REG_CTRL3   , (uint8_t*)&ctrl3   , 1) ||
        !lis3dh_reg_read (dev, LIS3DH_REG_STATUS  , (uint8_t*)&status  , 1) ||
        !lis3dh_reg_read (dev, LIS3DH_REG_FIFO_SRC, (uint8_t*)&fifo_src, 1))
    {   
        error_dev ("Could not read source of interrupt INT2 from sensor", __FUNCTION__, dev);
        dev->error_code |= LIS3DH_INT_SOURCE_FAILED;
        return false;
    }

    source->data_ready     = status.ZYXDA & ctrl3.IT_DRDY1;
    source->fifo_watermark = fifo_src.WTM & ctrl3.I1_WTM1;
    source->fifo_overrun   = fifo_src.OVRN_FIFO & ctrl3.I1_OVERRUN;

    return true;
}


bool lis3dh_set_int_event_config (lis3dh_sensor_t* dev,
                                  lis3dh_int_event_config_t* config,
                                  lis3dh_int_event_gen_t gen)
{
    if (!dev || !config) return false;

    dev->error_code = LIS3DH_OK;

    struct lis3dh_reg_intx_cfg intx_cfg;

    intx_cfg.XLIE = config->x_low_enabled;
    intx_cfg.XHIE = config->x_high_enabled;

    intx_cfg.YLIE = config->y_low_enabled;
    intx_cfg.YHIE = config->y_high_enabled;

    intx_cfg.ZLIE = config->z_low_enabled;
    intx_cfg.ZHIE = config->z_high_enabled;
    
    bool d4d_int = false;
    
    switch (config->mode)
    {
        case lis3dh_wake_up     : intx_cfg.AOI = 0; intx_cfg.SIXD = 0; break;
        case lis3dh_free_fall   : intx_cfg.AOI = 1; intx_cfg.SIXD = 0; break;

        case lis3dh_4d_movement : d4d_int = true;
        case lis3dh_6d_movement : intx_cfg.AOI = 0; intx_cfg.SIXD = 1; break;

        case lis3dh_4d_position : d4d_int = true;
        case lis3dh_6d_position : intx_cfg.AOI = 1; intx_cfg.SIXD = 1; break;
    }

    uint8_t intx_cfg_addr = (gen == lis3dh_int_event1_gen) ? LIS3DH_REG_INT1_CFG : LIS3DH_REG_INT2_CFG;
    uint8_t intx_ths_addr = (gen == lis3dh_int_event1_gen) ? LIS3DH_REG_INT1_THS : LIS3DH_REG_INT2_THS;
    uint8_t intx_dur_addr = (gen == lis3dh_int_event1_gen) ? LIS3DH_REG_INT1_DUR : LIS3DH_REG_INT2_DUR;

    if (// write the thresholds to registers IG_THS_*
        !lis3dh_reg_write (dev, intx_ths_addr, &config->threshold, 1) ||
        
        // write duration configuration to IG_DURATION 
        !lis3dh_reg_write (dev, intx_dur_addr, &config->duration, 1) ||
        
        // write INT1 configuration  to IG_CFG
        !lis3dh_reg_write (dev, intx_cfg_addr, (uint8_t*)&intx_cfg, 1))
    {   
        error_dev ("Could not configure interrupt INT1", __FUNCTION__, dev);
        dev->error_code |= LIS3DH_CONFIG_INT_FAILED;
        return false;
    }
    
    if (gen == lis3dh_int_event1_gen)
    {
        lis3dh_update_reg (dev, LIS3DH_REG_CTRL5, lis3dh_reg_ctrl5, LIR_INT1, config->latch);
        lis3dh_update_reg (dev, LIS3DH_REG_CTRL5, lis3dh_reg_ctrl5, D4D_INT1, d4d_int);
    }
    else
    {
        lis3dh_update_reg (dev, LIS3DH_REG_CTRL5, lis3dh_reg_ctrl5, LIR_INT2, config->latch);
        lis3dh_update_reg (dev, LIS3DH_REG_CTRL5, lis3dh_reg_ctrl5, D4D_INT2, d4d_int);
    }
        
    return true;
}


bool lis3dh_get_int_event_config (lis3dh_sensor_t* dev,
                                  lis3dh_int_event_config_t* config,
                                  lis3dh_int_event_gen_t gen)
{
    if (!dev || !config) return false;

    dev->error_code = LIS3DH_OK;

    uint8_t intx_cfg_addr = (gen == lis3dh_int_event1_gen) ? LIS3DH_REG_INT1_CFG : LIS3DH_REG_INT2_CFG;
    uint8_t intx_ths_addr = (gen == lis3dh_int_event1_gen) ? LIS3DH_REG_INT1_THS : LIS3DH_REG_INT2_THS;
    uint8_t intx_dur_addr = (gen == lis3dh_int_event1_gen) ? LIS3DH_REG_INT1_DUR : LIS3DH_REG_INT2_DUR;

    struct lis3dh_reg_intx_cfg intx_cfg;
    struct lis3dh_reg_ctrl5    ctrl5;

    if (!lis3dh_reg_read (dev, intx_cfg_addr, (uint8_t*)&intx_cfg, 1) ||
        !lis3dh_reg_read (dev, intx_ths_addr, (uint8_t*)&config->threshold, 1) ||
        !lis3dh_reg_read (dev, intx_dur_addr, (uint8_t*)&config->duration, 1) ||
        !lis3dh_reg_read (dev, LIS3DH_REG_CTRL5, (uint8_t*)&ctrl5, 1))
    {   
        error_dev ("Could not read interrupt configuration from sensor", __FUNCTION__, dev);
        dev->error_code |= LIS3DH_CONFIG_INT_FAILED;
        return false;
    }

    config->x_low_enabled  = intx_cfg.XLIE;
    config->x_high_enabled = intx_cfg.XHIE;

    config->y_low_enabled  = intx_cfg.YLIE;
    config->y_high_enabled = intx_cfg.YHIE;

    config->z_low_enabled  = intx_cfg.ZLIE;
    config->z_high_enabled = intx_cfg.ZHIE;
    
    bool d4d_int = false;

    if (gen == lis3dh_int_event1_gen)
    {
        config->latch = ctrl5.LIR_INT1;
        d4d_int = ctrl5.D4D_INT1;
    }
    else
    {
        config->latch = ctrl5.LIR_INT2;
        d4d_int = ctrl5.D4D_INT2;
    }
    
    if (intx_cfg.AOI)
    {
        if (intx_cfg.SIXD && d4d_int)
            config->mode = lis3dh_4d_position;
        else if (intx_cfg.SIXD && !d4d_int)
            config->mode = lis3dh_6d_position;
        else
            config->mode = lis3dh_free_fall;
    }
    else
    {
        if (intx_cfg.SIXD && d4d_int)
            config->mode = lis3dh_4d_movement;
        else if (intx_cfg.SIXD && !d4d_int)
            config->mode = lis3dh_6d_movement;
        else
            config->mode = lis3dh_wake_up;
    }

    return true;
}


bool lis3dh_get_int_event_source (lis3dh_sensor_t* dev,
                                  lis3dh_int_event_source_t* source,
                                  lis3dh_int_event_gen_t gen)
{
    if (!dev || !source) return false;

    dev->error_code = LIS3DH_OK;

    struct lis3dh_reg_intx_cfg intx_cfg;
    struct lis3dh_reg_intx_src intx_src;
    
    uint8_t intx_cfg_addr = (gen == lis3dh_int_event1_gen) ? LIS3DH_REG_INT1_CFG : LIS3DH_REG_INT2_CFG;
    uint8_t intx_src_addr = (gen == lis3dh_int_event1_gen) ? LIS3DH_REG_INT1_SRC : LIS3DH_REG_INT2_SRC;

    if (!lis3dh_reg_read (dev, intx_src_addr, (uint8_t*)&intx_src, 1) ||
        !lis3dh_reg_read (dev, intx_cfg_addr, (uint8_t*)&intx_cfg, 1))
    {   
        error_dev ("Could not read source of interrupt INT1/INT2 from sensor", __FUNCTION__, dev);
        dev->error_code |= LIS3DH_INT_SOURCE_FAILED;
        return false;
    }

    source->active = intx_src.IA;
    source->x_low  = intx_src.XL & intx_cfg.XLIE;
    source->x_high = intx_src.XH & intx_cfg.XHIE;
    source->y_low  = intx_src.YL & intx_cfg.YLIE;
    source->y_high = intx_src.YH & intx_cfg.YHIE;
    source->z_low  = intx_src.ZL & intx_cfg.ZLIE;
    source->z_high = intx_src.ZH & intx_cfg.ZHIE;
    
    return true;
}


bool lis3dh_set_int_click_config (lis3dh_sensor_t* dev,
                                  lis3dh_int_click_config_t* config)
{
    if (!dev || !config) return false;

    dev->error_code = LIS3DH_OK;

    struct lis3dh_reg_click_cfg  click_cfg;

    click_cfg.XS = config->x_single;
    click_cfg.XD = config->x_double;

    click_cfg.YS = config->y_single;
    click_cfg.YD = config->y_double;

    click_cfg.ZS = config->z_single;
    click_cfg.ZD = config->z_double;
    
    uint8_t click_ths = config->threshold | ((config->latch) ? 0x80 : 0x00);

    if (!lis3dh_reg_write (dev, LIS3DH_REG_CLICK_CFG   , (uint8_t*)&click_cfg, 1) ||
        !lis3dh_reg_write (dev, LIS3DH_REG_CLICK_THS   , (uint8_t*)&click_ths, 1) ||
        !lis3dh_reg_write (dev, LIS3DH_REG_TIME_LIMIT  , (uint8_t*)&config->time_limit, 1) ||
        !lis3dh_reg_write (dev, LIS3DH_REG_TIME_LATENCY, (uint8_t*)&config->time_latency, 1) ||
        !lis3dh_reg_write (dev, LIS3DH_REG_TIME_WINDOW , (uint8_t*)&config->time_window, 1))
    {   
        error_dev ("Could not configure click detection interrupt", __FUNCTION__, dev);
        dev->error_code |= LIS3DH_CONFIG_CLICK_FAILED;
        return false;
    }
    
    return true;
}

bool lis3dh_get_int_click_config (lis3dh_sensor_t* dev,
                                  lis3dh_int_click_config_t* config)
{
    if (!dev || !config) return false;

    dev->error_code = LIS3DH_OK;

    struct lis3dh_reg_click_cfg  click_cfg;
    uint8_t click_ths;

    if (!lis3dh_reg_read (dev, LIS3DH_REG_CLICK_CFG   , (uint8_t*)&click_cfg, 1) ||
        !lis3dh_reg_read (dev, LIS3DH_REG_CLICK_THS   , (uint8_t*)&click_ths, 1) ||
        !lis3dh_reg_read (dev, LIS3DH_REG_TIME_LIMIT  , (uint8_t*)&config->time_limit, 1) ||
        !lis3dh_reg_read (dev, LIS3DH_REG_TIME_LATENCY, (uint8_t*)&config->time_latency, 1) ||
        !lis3dh_reg_read (dev, LIS3DH_REG_TIME_WINDOW , (uint8_t*)&config->time_window, 1))
    {   
        error_dev ("Could not configure click detection interrupt", __FUNCTION__, dev);
        dev->error_code |= LIS3DH_CONFIG_CLICK_FAILED;
        return false;
    }
    
    config->x_single = click_cfg.XS;
    config->x_double = click_cfg.XD;

    config->y_single = click_cfg.YS;
    config->y_double = click_cfg.YD;

    config->z_single = click_cfg.ZS;
    config->z_double = click_cfg.ZD;
 
    config->threshold= click_ths & 0x7f;
    config->latch    = click_ths & 0x80;     
    
    return true;
}

bool lis3dh_get_int_click_source (lis3dh_sensor_t* dev,
                                  lis3dh_int_click_source_t* source)
{
    if (!dev || !source) return false;

    dev->error_code = LIS3DH_OK;

    if (!lis3dh_reg_read (dev, LIS3DH_REG_CLICK_SRC, (uint8_t*)source, 1))
    {   
        error_dev ("Could not read source of click interrupt from sensor", __FUNCTION__, dev);
        dev->error_code |= LIS3DH_CLICK_SOURCE_FAILED;
        return false;
    }

    return true;
}
                                     


bool lis3dh_config_int_signals (lis3dh_sensor_t* dev, lis3dh_int_signal_level_t level)
{
    if (!dev) return false;

    dev->error_code = LIS3DH_OK;

    lis3dh_update_reg (dev, LIS3DH_REG_CTRL6, lis3dh_reg_ctrl6, H_LACTIVE, level);
    
    return true;
}


bool lis3dh_config_hpf (lis3dh_sensor_t* dev, 
                        lis3dh_hpf_mode_t mode, uint8_t cutoff,
                        bool data, bool click, bool int1, bool int2)
{
    if (!dev) return false;

    dev->error_code = LIS3DH_OK;

    struct lis3dh_reg_ctrl2 reg;
    
    reg.HPM  = mode;
    reg.HPCF = cutoff;
    reg.FDS  = data;
    reg.HPCLICK = click;
    reg.HPIS1   = int1;
    reg.HPIS2   = int2;
    
    if (!lis3dh_reg_write (dev, LIS3DH_REG_CTRL2, (uint8_t*)&reg, 1))
    {   
        error_dev ("Could not configure high pass filter", __FUNCTION__, dev);
        dev->error_code |= LIS3DH_CONFIG_HPF_FAILED;
        return false;
    }

    return true;
}


bool lis3dh_set_hpf_ref (lis3dh_sensor_t* dev, int8_t ref)
{
    if (!dev) return false;

    dev->error_code = LIS3DH_OK;

    if (!lis3dh_reg_write (dev, LIS3DH_REG_REFERENCE, (uint8_t*)&ref, 1))
    {   
        error_dev ("Could not set high pass filter reference", __FUNCTION__, dev);
        dev->error_code |= LIS3DH_CONFIG_HPF_FAILED;
        return false;
    }

    return true;
}


int8_t lis3dh_get_hpf_ref (lis3dh_sensor_t* dev)
{
    if (!dev) return 0;

    dev->error_code = LIS3DH_OK;

    int8_t ref;
    
    if (!lis3dh_reg_read (dev, LIS3DH_REG_REFERENCE, (uint8_t*)&ref, 1))
    {   
        error_dev ("Could not get high pass filter reference", __FUNCTION__, dev);
        dev->error_code |= LIS3DH_CONFIG_HPF_FAILED;
        return 0;
    }

    return ref;
}

int8_t lis3dh_enable_adc (lis3dh_sensor_t* dev, bool adc, bool tmp)
{
    if (!dev) return 0;

    dev->error_code = LIS3DH_OK;

    uint8_t reg = 0;
    
    reg |= (adc) ? 0x80 : 0;
    reg |= (tmp) ? 0x40 : 0;
    
    return lis3dh_reg_write (dev, LIS3DH_REG_TEMP_CFG, (uint8_t*)&reg, 1);  
}


bool lis3dh_get_adc (lis3dh_sensor_t* dev,
                     uint16_t* adc1, uint16_t* adc2, uint16_t* adc3)
{
    if (!dev) return false;

    dev->error_code = LIS3DH_OK;

    uint8_t data[6];
    uint8_t temp_cfg;
    struct lis3dh_reg_ctrl1 ctrl1;

    if (!lis3dh_reg_read (dev, LIS3DH_REG_OUT_ADC1_L, data, 6) ||
        !lis3dh_reg_read (dev, LIS3DH_REG_CTRL1, (uint8_t*)&ctrl1, 1) ||
        !lis3dh_reg_read (dev, LIS3DH_REG_TEMP_CFG, &temp_cfg, 1))
    {
        error_dev ("Could not get adc data", __FUNCTION__, dev);
        dev->error_code |= LIS3DH_GET_ADC_DATA_FAILED;
        return false;
    }

    if (adc1) *adc1 = lsb_msb_to_type ( int16_t, data, 0) >> (ctrl1.LPen ? 8 : 6);
    if (adc2) *adc2 = lsb_msb_to_type ( int16_t, data, 2) >> (ctrl1.LPen ? 8 : 6);
    
    // temperature is always 8 bit
    if (adc3 && temp_cfg & 0x40)
        *adc3 = (lsb_msb_to_type ( int16_t, data, 4) >> 8) + 25;
    else if (adc3)
        *adc3 = lsb_msb_to_type ( int16_t, data, 4) >> (ctrl1.LPen ? 8 : 6);
        
    return true;
}


/** Functions for internal use only */

/**
 * @brief   Check the chip ID to test whether sensor is available
 */
static bool lis3dh_is_available (lis3dh_sensor_t* dev)
{
    uint8_t chip_id;

    if (!dev) return false;

    dev->error_code = LIS3DH_OK;

    if (!lis3dh_reg_read (dev, LIS3DH_REG_WHO_AM_I, &chip_id, 1))
        return false;

    if (chip_id != LIS3DH_CHIP_ID)
    {
        error_dev ("Chip id %02x is wrong, should be %02x.",
                    __FUNCTION__, dev, chip_id, LIS3DH_CHIP_ID);
        dev->error_code = LIS3DH_WRONG_CHIP_ID;
        return false;
    }

    return true;
}

static bool lis3dh_reset (lis3dh_sensor_t* dev)
{
    if (!dev) return false;

    dev->error_code = LIS3DH_OK;

    uint8_t reg[8] = { 0 };
    
    // initialize sensor completely including setting in power down mode
    lis3dh_reg_write (dev, LIS3DH_REG_TEMP_CFG , reg, 8);
    lis3dh_reg_write (dev, LIS3DH_REG_FIFO_CTRL, reg, 1);
    lis3dh_reg_write (dev, LIS3DH_REG_INT1_CFG , reg, 1);
    lis3dh_reg_write (dev, LIS3DH_REG_INT1_THS , reg, 2);
    lis3dh_reg_write (dev, LIS3DH_REG_INT2_CFG , reg, 1);
    lis3dh_reg_write (dev, LIS3DH_REG_INT2_THS , reg, 2);
    lis3dh_reg_write (dev, LIS3DH_REG_CLICK_CFG, reg, 1);
    lis3dh_reg_write (dev, LIS3DH_REG_CLICK_THS, reg, 4);
    
    return true;
}


bool lis3dh_reg_read(lis3dh_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len)
{
    if (!dev || !data) return false;

    return (dev->addr) ? lis3dh_i2c_read (dev, reg, data, len)
                       : lis3dh_spi_read (dev, reg, data, len);
}


bool lis3dh_reg_write(lis3dh_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len)
{
    if (!dev || !data) return false;

    return (dev->addr) ? lis3dh_i2c_write (dev, reg, data, len)
                       : lis3dh_spi_write (dev, reg, data, len);
}


#define LIS3DH_SPI_BUF_SIZE 64      // SPI register data buffer size of ESP866

#define LIS3DH_SPI_READ_FLAG      0x80
#define LIS3DH_SPI_WRITE_FLAG     0x00
#define LIS3DH_SPI_AUTO_INC_FLAG  0x40

static bool lis3dh_spi_read(lis3dh_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len)
{
    if (!dev || !data) return false;

    if (len >= LIS3DH_SPI_BUF_SIZE)
    {
        dev->error_code |= LIS3DH_SPI_BUFFER_OVERFLOW;
        error_dev ("Error on read from SPI slave on bus 1. Tried to transfer "
                   "more than %d byte in one read operation.",
                   __FUNCTION__, dev, LIS3DH_SPI_BUF_SIZE);
        return false;
    }

    uint8_t addr = (reg & 0x3f) | LIS3DH_SPI_READ_FLAG | LIS3DH_SPI_AUTO_INC_FLAG;
    
    static uint8_t mosi[LIS3DH_SPI_BUF_SIZE];
    static uint8_t miso[LIS3DH_SPI_BUF_SIZE];

    memset (mosi, 0xff, LIS3DH_SPI_BUF_SIZE);
    memset (miso, 0xff, LIS3DH_SPI_BUF_SIZE);

    mosi[0] = addr;
    
    if (!spi_transfer_pf (dev->bus, dev->cs, mosi, miso, len+1))
    {
        error_dev ("Could not read data from SPI", __FUNCTION__, dev);
        dev->error_code |= LIS3DH_SPI_READ_FAILED;
        return false;
    }
    
    // shift data one by left, first byte received while sending register address is invalid
    for (int i=0; i < len; i++)
      data[i] = miso[i+1];

    #ifdef LIS3DH_DEBUG_LEVEL_2
    printf("LIS3DH %s: read the following bytes from reg %02x: ", __FUNCTION__, reg);
    for (int i=0; i < len; i++)
        printf("%02x ", data[i]);
    printf("\n");
    #endif

    return true;
}


static bool lis3dh_spi_write(lis3dh_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len)
{
    if (!dev || !data) return false;

    uint8_t addr = (reg & 0x3f) | LIS3DH_SPI_WRITE_FLAG | LIS3DH_SPI_AUTO_INC_FLAG;

    static uint8_t mosi[LIS3DH_SPI_BUF_SIZE];

    if (len >= LIS3DH_SPI_BUF_SIZE)
    {
        dev->error_code |= LIS3DH_SPI_BUFFER_OVERFLOW;
        error_dev ("Error on write to SPI slave on bus 1. Tried to transfer more"
                   "than %d byte in one write operation.", 
                   __FUNCTION__, dev, LIS3DH_SPI_BUF_SIZE);

        return false;
    }

    reg &= 0x7f;

    // first byte in output is the register address
    mosi[0] = addr;

    // shift data one byte right, first byte in output is the register address
    for (int i = 0; i < len; i++)
        mosi[i+1] = data[i];

    #ifdef LIS3DH_DEBUG_LEVEL_2
    printf("LIS3DH %s: Write the following bytes to reg %02x: ", __FUNCTION__, reg);
    for (int i = 1; i < len+1; i++)
        printf("%02x ", mosi[i]);
    printf("\n");
    #endif

    if (!spi_transfer_pf (dev->bus, dev->cs, mosi, NULL, len+1))
    {
        error_dev ("Could not write data to SPI.", __FUNCTION__, dev);
        dev->error_code |= LIS3DH_SPI_WRITE_FAILED;
        return false;
    }

    return true;
}


#define I2C_AUTO_INCREMENT (0x80)

static bool lis3dh_i2c_read(lis3dh_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len)
{
    if (!dev || !data) return false;

    debug_dev ("Read %d byte from i2c slave register %02x.", __FUNCTION__, dev, len, reg);

    if (len > 1)
        reg |= I2C_AUTO_INCREMENT;
    
    int result = i2c_slave_read(dev->bus, dev->addr, &reg, data, len);

    if (result)
    {
        dev->error_code |= (result == -EBUSY) ? LIS3DH_I2C_BUSY : LIS3DH_I2C_READ_FAILED;
        error_dev ("Error %d on read %d byte from I2C slave register %02x.",
                    __FUNCTION__, dev, result, len, reg);
        return false;
    }

#   ifdef LIS3DH_DEBUG_LEVEL_2
    printf("LIS3DH %s: Read following bytes: ", __FUNCTION__);
    printf("%02x: ", reg & 0x7f);
    for (int i=0; i < len; i++)
        printf("%02x ", data[i]);
    printf("\n");
#   endif

    return true;
}


static bool lis3dh_i2c_write(lis3dh_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len)
{
    if (!dev || !data) return false;

    debug_dev ("Write %d byte to i2c slave register %02x.", __FUNCTION__, dev, len, reg);

    if (len > 1)
        reg |= I2C_AUTO_INCREMENT;

    int result = i2c_slave_write(dev->bus, dev->addr, &reg, data, len);

    if (result)
    {
        dev->error_code |= (result == -EBUSY) ? LIS3DH_I2C_BUSY : LIS3DH_I2C_WRITE_FAILED;
        error_dev ("Error %d on write %d byte to i2c slave register %02x.",
                    __FUNCTION__, dev, result, len, reg);
        return false;
    }

#   ifdef LIS3DH_DEBUG_LEVEL_2
    printf("LIS3DH %s: Wrote the following bytes: ", __FUNCTION__);
    printf("%02x: ", reg & 0x7f);
    for (int i=0; i < len; i++)
        printf("%02x ", data[i]);
    printf("\n");
#   endif

    return true;
}
