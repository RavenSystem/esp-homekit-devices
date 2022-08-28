/*
 * Driver for LIS3MDL 3-axes digital magnetometer connected to I2C or SPI.
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

#include "lis3mdl.h"

#if defined(LIS3MDL_DEBUG_LEVEL_2)
#define debug(s, f, ...) printf("%s %s: " s "\n", "LIS3MDL", f, ## __VA_ARGS__)
#define debug_dev(s, f, d, ...) printf("%s %s: bus %d, addr %02x - " s "\n", "LIS3MDL", f, d->bus, d->addr, ## __VA_ARGS__)
#else
#define debug(s, f, ...)
#define debug_dev(s, f, d, ...)
#endif

#if defined(LIS3MDL_DEBUG_LEVEL_1) || defined(LIS3MDL_DEBUG_LEVEL_2)
#define error(s, f, ...) printf("%s %s: " s "\n", "LIS3MDL", f, ## __VA_ARGS__)
#define error_dev(s, f, d, ...) printf("%s %s: bus %d, addr %02x - " s "\n", "LIS3MDL", f, d->bus, d->addr, ## __VA_ARGS__)
#else
#define error(s, f, ...)
#define error_dev(s, f, d, ...)
#endif

// register addresses
#define LIS3MDL_REG_WHO_AM_I      0x0f
#define LIS3MDL_REG_CTRL1         0x20
#define LIS3MDL_REG_CTRL2         0x21
#define LIS3MDL_REG_CTRL3         0x22
#define LIS3MDL_REG_CTRL4         0x23
#define LIS3MDL_REG_CTRL5         0x24
#define LIS3MDL_REG_STATUS        0x27
#define LIS3MDL_REG_OUT_X_L       0x28
#define LIS3MDL_REG_OUT_X_H       0x29
#define LIS3MDL_REG_OUT_Y_L       0x2a
#define LIS3MDL_REG_OUT_Y_H       0x2b
#define LIS3MDL_REG_OUT_Z_L       0x2c
#define LIS3MDL_REG_OUT_Z_H       0x2d
#define LIS3MDL_REG_TEMP_OUT_L    0x2e
#define LIS3MDL_REG_TEMP_OUT_H    0x2f
#define LIS3MDL_REG_INT_CFG       0x30
#define LIS3MDL_REG_INT_SRC       0x31
#define LIS3MDL_REG_INT_THS_L     0x32
#define LIS3MDL_REG_INT_THS_H     0x33

// register structure definitions
struct lis3mdl_reg_status 
{
    uint8_t XDA      :1;    // STATUS<0>   X axis new data available
    uint8_t YDA      :1;    // STATUS<1>   Y axis new data available
    uint8_t ZDA      :1;    // STATUS<2>   Z axis new data available
    uint8_t ZYXDA    :1;    // STATUS<3>   X, Y and Z axis new data available
    uint8_t XOR      :1;    // STATUS<4>   X axis data overrun
    uint8_t YOR      :1;    // STATUS<5>   Y axis data overrun 
    uint8_t ZOR      :1;    // STATUS<6>   Z axis data overrun
    uint8_t ZYXOR    :1;    // STATUS<7>   X, Y and Z axis data overrun
};

#define LIS3MDL_ANY_DATA_READY    0x0f    // LIS3MDL_REG_STATUS<3:0>

struct lis3mdl_reg_ctrl1 
{
    uint8_t ST       :1;    // CTRL1<0>    Self-test enable
    uint8_t FAST_ODR :1;    // CTRL1<1>    Data rates higher 80 Hz enabled
    uint8_t DO       :3;    // CTRL1<4:2>  Output data rate
    uint8_t OM       :2;    // CTRL1<6:5>  X and Y axes operative mode
    uint8_t TEMP_EN  :1;    // CTRL1<7>    Temperature sensor enabled
};

struct lis3mdl_reg_ctrl2 
{
    uint8_t unused1  :2;    // CTRL2<1:0>  unused
    uint8_t SOFT_RST :1;    // CTRL2<2>    configuration and user regs reset
    uint8_t REBOOT   :1;    // CTRL2<3>    Reboot memory content
    uint8_t unused2  :1;    // CTRL2<4>    unused
    uint8_t FS       :2;    // CTRL2<6:5>  
    uint8_t unused3  :1;    // CTRL2<7>    unused
};

struct lis3mdl_reg_ctrl3 
{
    uint8_t MD       :2;    // CTRL3<1:0>  Operation mode selection
    uint8_t SIM      :1;    // CTRL3<2>    SPI serial interface mode selection
    uint8_t unused1  :2;    // CTRL3<4:3>  unused
    uint8_t LP       :1;    // CTRL3<5>    Low power mode configuration
    uint8_t unused2  :2;    // CTRL3<7:6>  unused
};

struct lis3mdl_reg_ctrl4 
{
    uint8_t unused1  :1;    // CTRL4<0>    unused
    uint8_t BLE      :1;    // CTRL4<1>    Big/litle endian data selection
    uint8_t OMZ      :2;    // CTRL4<3:2>  Z axis operative mode
    uint8_t unused2  :4;    // CTRL4<7:4>  unused
};

struct lis3mdl_reg_ctrl5 
{
    uint8_t unused   :6;    // CTRL5<5:0>  unused
    uint8_t BDU      :1;    // CTRL5<6>    Block data update
    uint8_t FAST_READ:1;    // CTRL5<7>    Fast read enabled
};


struct lis3mdl_reg_int_cfg
{
    uint8_t IEN      :1;    // INT_CFG<0>   Interrupt enabled
    uint8_t LIR      :1;    // INT_CFG<1>   Latch interrupt request
    uint8_t IEA      :1;    // INT_CFG<2>   Interrupt active
    uint8_t unused   :2;    // INT_CFG<4:3> unused
    uint8_t ZIEN     :1;    // INT_CFG<5>   Z axis threshold interrupt enabled
    uint8_t YIEN     :1;    // INT_CFG<6>   Y axis threshold interrupt enabled
    uint8_t XIEN     :1;    // INT_CFG<7>   X axis threshold interrupt enabled
};

struct lis3mdl_reg_int_src
{
    uint8_t PTH_X    :1;    // INT_SRC<0>   X exceeds threshold on positive side
    uint8_t PTH_Y    :1;    // INT_SRC<1>   Y exceeds threshold on positive side
    uint8_t PTH_Z    :1;    // INT_SRC<2>   Z exceeds threshold on positive side
    uint8_t NTH_X    :1;    // INT_SRC<3>   X exceeds threshold on negative side
    uint8_t NTH_Y    :1;    // INT_SRC<4>   Y exceeds threshold on negative side
    uint8_t NTH_Z    :1;    // INT_SRC<5>   Z exceeds threshold on negative side
    uint8_t MROI     :1;    // INT_SRC<6>   Internal measurement range overflow
    uint8_t INT      :1;    // INT_SRC<7>   Interrupt event occurs
};

/** Forward declaration of functions for internal use */

static bool    lis3mdl_reset       (lis3mdl_sensor_t* dev);
static bool    lis3mdl_is_available(lis3mdl_sensor_t* dev);

static bool    lis3mdl_i2c_read    (lis3mdl_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len);
static bool    lis3mdl_i2c_write   (lis3mdl_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len);
static bool    lis3mdl_spi_read    (lis3mdl_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len);
static bool    lis3mdl_spi_write   (lis3mdl_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len);

#define msb_lsb_to_type(t,b,o) (t)(((t)b[o] << 8) | b[o+1])
#define lsb_msb_to_type(t,b,o) (t)(((t)b[o+1] << 8) | b[o])
#define lsb_to_type(t,b,o)     (t)(b[o])

#define lis3mdl_update_reg(dev,addr,type,elem,value) \
        { \
            struct type __reg; \
            if (!lis3mdl_reg_read (dev, (addr), (uint8_t*)&__reg, 1)) \
                return false; \
            __reg.elem = (value); \
            if (!lis3mdl_reg_write (dev, (addr), (uint8_t*)&__reg, 1)) \
                return false; \
        }

lis3mdl_sensor_t* lis3mdl_init_sensor (uint8_t bus, uint8_t addr, uint8_t cs)
{
    lis3mdl_sensor_t* dev;

    if ((dev = malloc (sizeof(lis3mdl_sensor_t))) == NULL)
        return NULL;

    // init sensor data structure
    dev->bus    = bus;
    dev->addr   = addr;
    dev->cs     = cs;

    dev->error_code = LIS3MDL_OK;
    dev->scale      = lis3mdl_scale_4_Gs;
    
    // if addr==0 then SPI is used and has to be initialized
    if (!addr && !spi_device_init (bus, cs))
    {
        error_dev ("Could not initialize SPI interface.", __FUNCTION__, dev);
        free (dev);
        return NULL;
    }
        
    // check availability of the sensor
    if (!lis3mdl_is_available (dev))
    {
        error_dev ("Sensor is not available.", __FUNCTION__, dev);
        free (dev);
        return NULL;
    }

    // reset the sensor
    if (!lis3mdl_reset(dev))
    {
        error_dev ("Could not reset the sensor device.", __FUNCTION__, dev);
        free (dev);
        return NULL;
    }
    
    lis3mdl_update_reg (dev, LIS3MDL_REG_CTRL2, lis3mdl_reg_ctrl2, FS, lis3mdl_scale_4_Gs);
    lis3mdl_update_reg (dev, LIS3MDL_REG_CTRL5, lis3mdl_reg_ctrl5, BDU, 1);

    return dev;
}

// switching times
//   LP  0.90
//   MP  1.65
//   HP  3.23
//   UHP 6.40

bool lis3mdl_set_mode (lis3mdl_sensor_t* dev, lis3mdl_mode_t mode)
{
    if (!dev) return false;

    dev->error_code = LIS3MDL_OK;

    struct lis3mdl_reg_ctrl1 ctrl1;
    struct lis3mdl_reg_ctrl3 ctrl3;
    struct lis3mdl_reg_ctrl4 ctrl4;

    // read current register values
    if (!lis3mdl_reg_read (dev, LIS3MDL_REG_CTRL1, (uint8_t*)&ctrl1, 1) ||
        !lis3mdl_reg_read (dev, LIS3MDL_REG_CTRL3, (uint8_t*)&ctrl3, 1) ||
        !lis3mdl_reg_read (dev, LIS3MDL_REG_CTRL4, (uint8_t*)&ctrl4, 1))
        return false;

    if (mode < lis3mdl_lpm_1000)
    {
        ctrl1.FAST_ODR = 0;

        ctrl3.MD  = 0;  // continuous measurement
        ctrl3.LP  = 0;

        ctrl1.DO  = mode;
        ctrl1.OM  = 0;
        ctrl4.OMZ = ctrl1.OM;
    }
    else if (mode < lis3mdl_low_power)
    {
        ctrl1.FAST_ODR = 1;

        ctrl3.MD  = 0;  // continuous measurement
        ctrl3.LP  = 0;

        ctrl1.DO  = 0;
        ctrl1.OM  = mode - lis3mdl_lpm_1000;
        ctrl4.OMZ = ctrl1.OM;
    }
    else if (mode == lis3mdl_low_power)
    {
        ctrl1.FAST_ODR = 0;

        ctrl3.MD  = 0;  // continuous measurement
        ctrl3.LP  = 1;  // at lowest data rate 0.625 Hz

        ctrl1.DO  = 0;
        ctrl1.OM  = 0;
        ctrl4.OMZ = ctrl1.OM;
    }
    else // lis3mdl_power_down
    {
        ctrl3.MD =  3;
    }
    
    if (!lis3mdl_reg_write (dev, LIS3MDL_REG_CTRL1, (uint8_t*)&ctrl1, 1) ||
        !lis3mdl_reg_write (dev, LIS3MDL_REG_CTRL3, (uint8_t*)&ctrl3, 1) ||
        !lis3mdl_reg_write (dev, LIS3MDL_REG_CTRL4, (uint8_t*)&ctrl4, 1))
        return false;

    // wait until mode switch happened
    vTaskDelay (50/portTICK_PERIOD_MS);
    
    // dummy read last data register set
    lis3mdl_raw_data_t raw;
    lis3mdl_get_raw_data (dev, &raw);

    return false;
}


bool lis3mdl_set_scale (lis3mdl_sensor_t* dev, lis3mdl_scale_t scale)
{
    if (!dev) return false;
    
    dev->error_code = LIS3MDL_OK;
    dev->scale = scale;
    
    // read CTRL2 register and write scale
    lis3mdl_update_reg (dev, LIS3MDL_REG_CTRL2, lis3mdl_reg_ctrl2, FS, scale);
    
    return true;
}


bool lis3mdl_new_data (lis3mdl_sensor_t* dev)
{
    if (!dev) return false;

    dev->error_code = LIS3MDL_OK;

    struct lis3mdl_reg_status status;
        
    if (!lis3mdl_reg_read (dev, LIS3MDL_REG_STATUS, (uint8_t*)&status, 1))
    {
        error_dev ("Could not get sensor status", __FUNCTION__, dev);
        return false;
    }
    return status.ZYXDA;
}

/**
 * Scaling factors for the conversion of raw sensor data to floating point _Gs
 * values. Scaling factors are from mechanical characteristics in datasheet.
 *
 *  scale/sensitivity  resolution       sensitivity
 *       +-4 gauss     6842 LSB/gauss   1.461561e-4
 *       +-8 gauss     3421 LSB/gauss   2,923122e-4
 *      +-12 gauss     2281 LSB/gauss   4,384042e-4
 *      +-16 gauss     1711 LSB/gauss   5,844535e-4
 */
const static double  LIS3MDL_SCALES[4] = { 1.0/6842, 1.0/3421, 1.0/2281, 1.0/1711 };

bool lis3mdl_get_float_data (lis3mdl_sensor_t* dev, lis3mdl_float_data_t* data)
{
    if (!dev || !data) return false;

    lis3mdl_raw_data_t raw;
    
    if (!lis3mdl_get_raw_data (dev, &raw))
        return false;

    data->mx = LIS3MDL_SCALES[dev->scale] * raw.mx;
    data->my = LIS3MDL_SCALES[dev->scale] * raw.my;
    data->mz = LIS3MDL_SCALES[dev->scale] * raw.mz;

    return true;
}


bool lis3mdl_get_raw_data (lis3mdl_sensor_t* dev, lis3mdl_raw_data_t* raw)
{
    if (!dev || !raw) return false;

    dev->error_code = LIS3MDL_OK;

    uint8_t regs[6];

    // read raw data sample
    if (!lis3mdl_reg_read (dev, LIS3MDL_REG_OUT_X_L, regs, 6))
    {
        error_dev ("Could not get raw data sample", __FUNCTION__, dev);
        dev->error_code |= LIS3MDL_GET_RAW_DATA_FAILED;
        return false;
    }
    
    raw->mx = ((uint16_t)regs[1] << 8) | regs[0];
    raw->my = ((uint16_t)regs[3] << 8) | regs[2];
    raw->mz = ((uint16_t)regs[5] << 8) | regs[4];

    return true;
}


bool lis3mdl_set_int_config (lis3mdl_sensor_t* dev, 
                             lis3mdl_int_config_t* cfg)
{
    if (!dev || !cfg) return false;

    dev->error_code = LIS3MDL_OK;

    struct lis3mdl_reg_int_cfg int_cfg;
    
    int_cfg.unused = 0;

    int_cfg.XIEN = cfg->x_enabled;
    int_cfg.YIEN = cfg->y_enabled;
    int_cfg.ZIEN = cfg->z_enabled;

    int_cfg.LIR  = cfg->latch;
    int_cfg.IEA  = cfg->signal_level;
    int_cfg.IEN  = cfg->x_enabled | cfg->y_enabled | cfg->z_enabled;

    if (// write the threshold to registers INT_THS_*
        !lis3mdl_reg_write (dev, LIS3MDL_REG_INT_THS_L, (uint8_t*)&cfg->threshold, 2) ||
        
        // write configuration to INT_CFG 
        !lis3mdl_reg_write (dev, LIS3MDL_REG_INT_CFG, (uint8_t*)&int_cfg, 1))
    {   
        error_dev ("Could not configure interrupt INT", __FUNCTION__, dev);
        dev->error_code |= LIS3MDL_CONFIG_INT_FAILED;
        return false;
    }
        
    return true;
}


bool lis3mdl_get_int_config (lis3mdl_sensor_t* dev,
                             lis3mdl_int_config_t* cfg)
{
    if (!dev || !cfg) return false;

    dev->error_code = LIS3MDL_OK;

    struct lis3mdl_reg_int_cfg int_cfg;

    if (!lis3mdl_reg_read (dev, LIS3MDL_REG_INT_THS_L, (uint8_t*)&cfg->threshold, 2) ||
        !lis3mdl_reg_read (dev, LIS3MDL_REG_INT_CFG  , (uint8_t*)&int_cfg, 1))
    {   
        error_dev ("Could not read configuration of interrupt INT from sensor", __FUNCTION__, dev);
        dev->error_code |= LIS3MDL_CONFIG_INT_FAILED;
        return false;
    }

    cfg->x_enabled    = int_cfg.XIEN;
    cfg->y_enabled    = int_cfg.YIEN;
    cfg->z_enabled    = int_cfg.ZIEN;

    cfg->latch        = int_cfg.LIR;
    cfg->signal_level = int_cfg.IEA;

    return true;
}


bool lis3mdl_get_int_source (lis3mdl_sensor_t* dev,
                             lis3mdl_int_source_t* src)
{
    if (!dev || !src) return false;

    dev->error_code = LIS3MDL_OK;

    struct lis3mdl_reg_int_src int_src;
    struct lis3mdl_reg_int_cfg int_cfg;
    
    if (!lis3mdl_reg_read (dev, LIS3MDL_REG_INT_SRC, (uint8_t*)&int_src, 1) ||
        !lis3mdl_reg_read (dev, LIS3MDL_REG_INT_CFG, (uint8_t*)&int_cfg, 1))
    {   
        error_dev ("Could not read source of interrupt INT from sensor", __FUNCTION__, dev);
        dev->error_code |= LIS3MDL_INT_SOURCE_FAILED;
        return false;
    }

    src->active = int_src.INT;

    src->x_pos  = int_src.PTH_X & int_cfg.XIEN;
    src->x_neg  = int_src.NTH_X & int_cfg.XIEN;

    src->y_pos  = int_src.PTH_Y & int_cfg.YIEN;
    src->y_neg  = int_src.NTH_Y & int_cfg.YIEN;

    src->z_pos  = int_src.PTH_Z & int_cfg.ZIEN;
    src->z_neg  = int_src.NTH_Z & int_cfg.ZIEN;

    return true;
}


bool lis3mdl_enable_temperature (lis3mdl_sensor_t* dev, bool enable)
{
    lis3mdl_update_reg (dev, LIS3MDL_REG_CTRL1, lis3mdl_reg_ctrl1, TEMP_EN, enable);
    
    return true;
}

float lis3mdl_get_temperature (lis3mdl_sensor_t* dev)
{
    uint8_t regs[2];

    // read raw data sample
    if (!lis3mdl_reg_read (dev, LIS3MDL_REG_TEMP_OUT_L, regs, 2))
    {
        error_dev ("Could not get temperature data sample", __FUNCTION__, dev);
        dev->error_code |= LIS3MDL_GET_RAW_DATA_FAILED;
        return false;
    }
    
    return (((int16_t)((regs[1] << 8) | regs[0])) >> 3) + 25.0;
}

/** Functions for internal use only */

/**
 * @brief   Check the chip ID to test whether sensor is available
 */
static bool lis3mdl_is_available (lis3mdl_sensor_t* dev)
{
    uint8_t chip_id;

    if (!dev) return false;

    dev->error_code = LIS3MDL_OK;

    if (!lis3mdl_reg_read (dev, LIS3MDL_REG_WHO_AM_I, &chip_id, 1))
        return false;

    if (chip_id != LIS3MDL_CHIP_ID)
    {
        error_dev ("Chip id %02x is wrong, should be %02x.",
                    __FUNCTION__, dev, chip_id, LIS3MDL_CHIP_ID);
        dev->error_code = LIS3MDL_WRONG_CHIP_ID;
        return false;
    }

    return true;
}

static bool lis3mdl_reset (lis3mdl_sensor_t* dev)
{
    if (!dev) return false;

    dev->error_code = LIS3MDL_OK;

    uint8_t ctrl_regs[5] = { 0x10, 0x00, 0x03, 0x00, 0x00 };
    uint8_t int_cfg = 0x00;
    
    // initialize sensor completely including setting in power down mode
    lis3mdl_reg_write (dev, LIS3MDL_REG_CTRL1  , ctrl_regs, 5);
    lis3mdl_reg_write (dev, LIS3MDL_REG_INT_CFG, &int_cfg , 1);
    
    return true;
}


bool lis3mdl_reg_read(lis3mdl_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len)
{
    if (!dev || !data) return false;

    return (dev->addr) ? lis3mdl_i2c_read (dev, reg, data, len)
                       : lis3mdl_spi_read (dev, reg, data, len);
}


bool lis3mdl_reg_write(lis3mdl_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len)
{
    if (!dev || !data) return false;

    return (dev->addr) ? lis3mdl_i2c_write (dev, reg, data, len)
                       : lis3mdl_spi_write (dev, reg, data, len);
}


#define LIS3MDL_SPI_BUF_SIZE 64      // SPI register data buffer size of ESP866

#define LIS3MDL_SPI_READ_FLAG      0x80
#define LIS3MDL_SPI_WRITE_FLAG     0x00
#define LIS3MDL_SPI_AUTO_INC_FLAG  0x40

static bool lis3mdl_spi_read(lis3mdl_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len)
{
    if (!dev || !data) return false;

    if (len >= LIS3MDL_SPI_BUF_SIZE)
    {
        dev->error_code |= LIS3MDL_SPI_BUFFER_OVERFLOW;
        error_dev ("Error on read from SPI slave on bus 1. Tried to transfer "
                   "more than %d byte in one read operation.",
                   __FUNCTION__, dev, LIS3MDL_SPI_BUF_SIZE);
        return false;
    }

    uint8_t addr = (reg & 0x3f) | LIS3MDL_SPI_READ_FLAG | LIS3MDL_SPI_AUTO_INC_FLAG;
    
    static uint8_t mosi[LIS3MDL_SPI_BUF_SIZE];
    static uint8_t miso[LIS3MDL_SPI_BUF_SIZE];

    memset (mosi, 0xff, LIS3MDL_SPI_BUF_SIZE);
    memset (miso, 0xff, LIS3MDL_SPI_BUF_SIZE);

    mosi[0] = addr;
    
    if (!spi_transfer_pf (dev->bus, dev->cs, mosi, miso, len+1))
    {
        error_dev ("Could not read data from SPI", __FUNCTION__, dev);
        dev->error_code |= LIS3MDL_SPI_READ_FAILED;
        return false;
    }
    
    // shift data one by left, first byte received while sending register address is invalid
    for (int i=0; i < len; i++)
      data[i] = miso[i+1];

    #ifdef LIS3MDL_DEBUG_LEVEL_2
    printf("LIS3MDL %s: read the following bytes from reg %02x: ", __FUNCTION__, reg);
    for (int i=0; i < len; i++)
        printf("%02x ", data[i]);
    printf("\n");
    #endif

    return true;
}


static bool lis3mdl_spi_write(lis3mdl_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len)
{
    if (!dev || !data) return false;

    uint8_t addr = (reg & 0x3f) | LIS3MDL_SPI_WRITE_FLAG | LIS3MDL_SPI_AUTO_INC_FLAG;

    static uint8_t mosi[LIS3MDL_SPI_BUF_SIZE];

    if (len >= LIS3MDL_SPI_BUF_SIZE)
    {
        dev->error_code |= LIS3MDL_SPI_BUFFER_OVERFLOW;
        error_dev ("Error on write to SPI slave on bus 1. Tried to transfer more"
                   "than %d byte in one write operation.", 
                   __FUNCTION__, dev, LIS3MDL_SPI_BUF_SIZE);

        return false;
    }

    reg &= 0x7f;

    // first byte in output is the register address
    mosi[0] = addr;

    // shift data one byte right, first byte in output is the register address
    for (int i = 0; i < len; i++)
        mosi[i+1] = data[i];

    #ifdef LIS3MDL_DEBUG_LEVEL_2
    printf("LIS3MDL %s: Write the following bytes to reg %02x: ", __FUNCTION__, reg);
    for (int i = 1; i < len+1; i++)
        printf("%02x ", mosi[i]);
    printf("\n");
    #endif

    if (!spi_transfer_pf (dev->bus, dev->cs, mosi, NULL, len+1))
    {
        error_dev ("Could not write data to SPI.", __FUNCTION__, dev);
        dev->error_code |= LIS3MDL_SPI_WRITE_FAILED;
        return false;
    }

    return true;
}


#define I2C_AUTO_INCREMENT (0x80)

static bool lis3mdl_i2c_read(lis3mdl_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len)
{
    if (!dev || !data) return false;

    debug_dev ("Read %d byte from i2c slave register %02x.", __FUNCTION__, dev, len, reg);

    if (len > 1)
        reg |= I2C_AUTO_INCREMENT;
    
    int result = i2c_slave_read(dev->bus, dev->addr, &reg, data, len);

    if (result)
    {
        dev->error_code |= (result == -EBUSY) ? LIS3MDL_I2C_BUSY : LIS3MDL_I2C_READ_FAILED;
        error_dev ("Error %d on read %d byte from I2C slave register %02x.",
                    __FUNCTION__, dev, result, len, reg);
        return false;
    }

#   ifdef LIS3MDL_DEBUG_LEVEL_2
    printf("LIS3MDL %s: Read following bytes: ", __FUNCTION__);
    printf("%02x: ", reg & 0x7f);
    for (int i=0; i < len; i++)
        printf("%02x ", data[i]);
    printf("\n");
#   endif

    return true;
}


static bool lis3mdl_i2c_write(lis3mdl_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len)
{
    if (!dev || !data) return false;

    debug_dev ("Write %d byte to i2c slave register %02x.", __FUNCTION__, dev, len, reg);

    if (len > 1)
        reg |= I2C_AUTO_INCREMENT;

    int result = i2c_slave_write(dev->bus, dev->addr, &reg, data, len);

    if (result)
    {
        dev->error_code |= (result == -EBUSY) ? LIS3MDL_I2C_BUSY : LIS3MDL_I2C_WRITE_FAILED;
        error_dev ("Error %d on write %d byte to i2c slave register %02x.",
                    __FUNCTION__, dev, result, len, reg);
        return false;
    }

#   ifdef LIS3MDL_DEBUG_LEVEL_2
    printf("LIS3MDL %s: Wrote the following bytes: ", __FUNCTION__);
    printf("%02x: ", reg & 0x7f);
    for (int i=0; i < len; i++)
        printf("%02x ", data[i]);
    printf("\n");
#   endif

    return true;
}
