/*
 * Driver for LSM303D 3-axes digital accelerometer and magnetometer connected
 * either to I2C or SPI.
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
 * Copyright (c) 2018 Gunar Schorcht (https://github.com/gschorcht)
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

#include "lsm303d.h"

#ifdef debug
#undef debug
#undef debug_dev
#endif

#ifdef error
#undef error
#undef error_dev
#endif

#if defined(LSM303D_DEBUG_LEVEL_2)
#define debug(s, f, ...) printf("%s %s: " s "\n", "LSM303D", f, ## __VA_ARGS__)
#define debug_dev(s, f, d, ...) printf("%s %s: bus %d, addr %02x - " s "\n", "LSM303D", f, d->bus, d->addr, ## __VA_ARGS__)
#else
#define debug(s, f, ...)
#define debug_dev(s, f, d, ...)
#endif

#if defined(LSM303D_DEBUG_LEVEL_1) || defined(LSM303D_DEBUG_LEVEL_2)
#define error(s, f, ...) printf("%s %s: " s "\n", "LSM303D", f, ## __VA_ARGS__)
#define error_dev(s, f, d, ...) printf("%s %s: bus %d, addr %02x - " s "\n", "LSM303D", f, d->bus, d->addr, ## __VA_ARGS__)
#else
#define error(s, f, ...)
#define error_dev(s, f, d, ...)
#endif

// -- register addresses ---------------------------

#define LSM303D_REG_TEMP_OUT_L    0x05
#define LSM303D_REG_TEMP_OUT_H    0x06

#define LSM303D_REG_STATUS_M      0x07
#define LSM303D_REG_OUT_X_L_M     0x08
#define LSM303D_REG_OUT_X_H_M     0x09
#define LSM303D_REG_OUT_Y_L_M     0x0a
#define LSM303D_REG_OUT_Y_H_M     0x0b
#define LSM303D_REG_OUT_Z_L_M     0x0c
#define LSM303D_REG_OUT_Z_H_M     0x0d

#define LSM303D_REG_WHO_AM_I      0x0f

#define LSM303D_REG_INT_CTRL_M    0x12
#define LSM303D_REG_INT_SRC_M     0x13
#define LSM303D_REG_INT_THS_L_M   0x14
#define LSM303D_REG_INT_THS_H_M   0x15
#define LSM303D_REG_OFFSET_X_L_M  0x16
#define LSM303D_REG_OFFSET_X_M_M  0x17
#define LSM303D_REG_OFFSET_Y_L_M  0x18
#define LSM303D_REG_OFFSET_Y_M_M  0x19
#define LSM303D_REG_OFFSET_Z_L_M  0x1a
#define LSM303D_REG_OFFSET_Z_M_M  0x1b

#define LSM303D_REG_REFERENCE_X   0x1c
#define LSM303D_REG_REFERENCE_Y   0x1d
#define LSM303D_REG_REFERENCE_Z   0x1e

#define LSM303D_REG_CTRL0         0x1f
#define LSM303D_REG_CTRL1         0x20
#define LSM303D_REG_CTRL2         0x21
#define LSM303D_REG_CTRL3         0x22
#define LSM303D_REG_CTRL4         0x23
#define LSM303D_REG_CTRL5         0x24
#define LSM303D_REG_CTRL6         0x25
#define LSM303D_REG_CTRL7         0x26

#define LSM303D_REG_STATUS_A      0x27
#define LSM303D_REG_OUT_X_L_A     0x28
#define LSM303D_REG_OUT_X_H_A     0x29
#define LSM303D_REG_OUT_Y_L_A     0x2a
#define LSM303D_REG_OUT_Y_H_A     0x2b
#define LSM303D_REG_OUT_Z_L_A     0x2c
#define LSM303D_REG_OUT_Z_H_A     0x2d

#define LSM303D_REG_FIFO_CTRL     0x2e
#define LSM303D_REG_FIFO_SRC      0x2f

#define LSM303D_REG_IG_CFG1       0x30
#define LSM303D_REG_IG_SRC1       0x31
#define LSM303D_REG_IG_THS1       0x32
#define LSM303D_REG_IG_DUR1       0x33
#define LSM303D_REG_IG_CFG2       0x34
#define LSM303D_REG_IG_SRC2       0x35
#define LSM303D_REG_IG_THS2       0x36
#define LSM303D_REG_IG_DUR2       0x37
#define LSM303D_REG_CLICK_CFG     0x38
#define LSM303D_REG_CLICK_SRC     0x39
#define LSM303D_REG_CLICK_THS     0x3a
#define LSM303D_REG_TIME_LIMIT    0x3b
#define LSM303D_REG_TIME_LATENCY  0x3c
#define LSM303D_REG_TIME_WINDOW   0x3d

// -- register structure definitions ---------------
//
// ACC = accelerator
// MAG = magnetometer

// magnetometer data status (LSM303D_REG_STATUS_M = 0x07)
struct lsm303d_reg_status_m
{
    uint8_t XMDA   :1;  // STATUS_M<0>   MAG X axis new data available
    uint8_t YMDA   :1;  // STATUS_M<1>   MAG Y axis new data available
    uint8_t ZMDA   :1;  // STATUS_M<2>   MAG Z axis new data available
    uint8_t ZYXMDA :1;  // STATUS_M<3>   MAG X, Y and Z axis new data available
    uint8_t XMOR   :1;  // STATUS_M<4>   MAG X axis data overrun
    uint8_t YMOR   :1;  // STATUS_M<5>   MAG Y axis data overrun 
    uint8_t ZMOR   :1;  // STATUS_M<6>   MAG Z axis data overrun
    uint8_t ZYXMOR :1;  // STATUS_M<7>   MAG X, Y and Z axis data overrun
};

#define LSM303D_ANY_M_DATA_READY    0x07    // LSM303D_REG_STATUS_M<3:0>

// accelerometer data status (LSM303D_REG_STATUS_A = 0x27)
struct lsm303d_reg_status_a
{
    uint8_t XADA  :1;   // STATUS_A<0>   ACC X axis new data available
    uint8_t YADA  :1;   // STATUS_A<1>   ACC Y axis new data available
    uint8_t ZADA  :1;   // STATUS_A<2>   ACC Z axis new data available
    uint8_t ZYXADA:1;   // STATUS_A<3>   ACC X, Y and Z axis new data available
    uint8_t XAOR  :1;   // STATUS_A<4>   ACC X axis data overrun
    uint8_t YAOR  :1;   // STATUS_A<5>   ACC Y axis data overrun 
    uint8_t ZAOR  :1;   // STATUS_A<6>   ACC Z axis data overrun
    uint8_t ZYXAOR:1;   // STATUS_A<7>   ACC X, Y and Z axis data overrun
};

#define LSM303D_ANY_A_DATA_READY    0x07    // LSM303D_REG_STATUS_A<3:0>

// MAG interrupt control register (LSM303D_REG_INT_CTRL_M = 0x12)
struct lsm303d_reg_int_ctrl_m
{
    uint8_t MIEN    :1; // INT_CTRL_M<0> Enable interrupt generation for magnetic data
    uint8_t D4D     :1; // INT_CTRL_M<1> 4D enable
    uint8_t MIEL    :1; // INT_CTRL_M<2> Latch interrupt request
    uint8_t MIEA    :1; // INT_CTRL_M<3> Interrupt polarity
    uint8_t PP_OD   :1; // INT_CTRL_M<4> Interrupt pin configuration
    uint8_t ZMIEN   :1; // INT_CTRL_M<5> Enable interrupt recognition for Z axis
    uint8_t YMIEN   :1; // INT_CTRL_M<6> Enable interrupt recognition for Y axis
    uint8_t XMIEN   :1; // INT_CTRL_M<7> Enable interrupt recognition for X axis
};

// MAG interrupt source register (LSM303D_REG_INT_SRC_M = 0x13)
struct lsm303d_reg_int_src_m
{
    uint8_t MINT    :1; // INT_SRC_M<0>  MAG interrupt event occurs
    uint8_t MROI    :1; // INT_SRC_M<1>  Internal measurement range overflow
    uint8_t M_NTH_Z :1; // INT_SRC_M<2>  MAG z value exceeds threshold on negative side
    uint8_t M_NTH_Y :1; // INT_SRC_M<3>  MAG y value exceeds threshold on negative side
    uint8_t M_NTH_X :1; // INT_SRC_M<4>  MAG x value exceeds threshold on negative side
    uint8_t M_PTH_Z :1; // INT_SRC_M<5>  MAG z value exceeds threshold on positive side
    uint8_t M_PTH_Y :1; // INT_SRC_M<6>  MAG y value exceeds threshold on positive side
    uint8_t M_PTH_X :1; // INT_SRC_M<7>  MAG x value exceeds threshold on positive side
};

// control register 0 (LSM303D_REG_CTRL1 = 0x1f)
struct lsm303d_reg_ctrl0 
{
    uint8_t HPIS2   :1; // CTRL0<0>      HPF enabled for interrupt generator 2
    uint8_t HPIS1   :1; // CTRL0<1>      HPF enabled for interrupt generator 1
    uint8_t HP_Click:1; // CTRL0<2>      HPF enabled for click detection
    uint8_t unused  :2; // CTRL0<4:3>    unused
    uint8_t FTH_EN  :1; // CTRL0<5>      FIFO programmable threshold enable
    uint8_t FIFO_EN :1; // CTRL0<6>      FIFO enable
    uint8_t BOOT    :1; // CTRL0<7>      Reboot memory content
};

// control register 1 (LSM303D_REG_CTRL1 = 0x20)
struct lsm303d_reg_ctrl1 
{
    uint8_t AXEN  :1;   // CTRL1<0>      ACC X axis enable
    uint8_t AYEN  :1;   // CTRL1<1>      ACC Y axis enable
    uint8_t AZEN  :1;   // CTRL1<2>      ACC Z axis enable
    uint8_t BDU   :1;   // CTRL1<3>      ACC and MAG block data update
    uint8_t AODR  :4;   // CTRL1<7:4>    ACC data rate selection
};

// control register 2 (LSM303D_REG_CTRL2 = 0x21)
struct lsm303d_reg_ctrl2 
{
    uint8_t SIM   :1;   // CTRL2<0>      SPI serial interface mode
    uint8_t AST   :1;   // CTRL2<1>      ACC self test enable
    uint8_t unused:1;   // CTRL2<2>      unused
    uint8_t AFS   :3;   // CTRL2<5:3>    ACC full scale selection
    uint8_t ABW   :2;   // CTRL2<7:6>    ACC anti-alias filter bandwidth selection
};

// control register 3 (LSM303D_REG_CTRL3 = 0x22)
struct lsm303d_reg_ctrl3 
{
    uint8_t INT1_EMPTY  :1; // CTRL3<0>  FIFO empty indication on INT1 enable
    uint8_t INT1_DRDY_M :1; // CTRL3<1>  MAG data ready signal on INT1 enable
    uint8_t INT1_DRDY_A :1; // CTRL3<2>  ACC data ready signal on INT1 enable
    uint8_t INT1_IGM    :1; // CTRL3<3>  MAG interrupt generator on INT1 enable
    uint8_t INT1_IG2    :1; // CTRL3<4>  ACC inertial interrupt generator 2 on INT1 enable
    uint8_t INT1_IG1    :1; // CTRL3<5>  ACC inertial interrupt generator 1 on INT1 enable
    uint8_t INT1_Click  :1; // CTRL3<6>  CLICK generator interrupt on INT1 enable
    uint8_t INT1_BOOT   :1; // CTRL3<7>  BOOT on INT1 enable
};

// control register 4 (LSM303D_REG_CTRL4 = 0x23)
struct lsm303d_reg_ctrl4
{
    uint8_t INT2_FTH    :1; // CTRL4<0>  FIFO threshold interrupt on INT2 enable
    uint8_t INT2_Overrun:1; // CTRL4<1>  FIFO Overrun interrupt on INT2
    uint8_t INT2_DRDY_M :1; // CTRL4<2>  MAG data ready signal on INT2 enable
    uint8_t INT2_DRDY_A :1; // CTRL4<3>  ACC data ready signal on INT2 enable
    uint8_t INT2_IGM    :1; // CTRL4<4>  MAG interrupt generator on INT2 enable
    uint8_t INT2_IG2    :1; // CTRL4<5>  ACC inertial interrupt generator 2 on INT2 enable
    uint8_t INT2_IG1    :1; // CTRL4<6>  ACC inertial interrupt generator 1 on INT2 enable
    uint8_t INT2_Click  :1; // CTRL4<7>  CLICK generator interrupt on INT2 enable
};

// control register 5 (LSM303D_REG_CTRL5 = 0x24)
struct lsm303d_reg_ctrl5
{
    uint8_t LIR1    :1;     // CTRL5<0>   Latch interrupt request on INT1
    uint8_t LIR2    :1;     // CTRL5<1>   Latch interrupt request on INT2
    uint8_t M_ODR   :3;     // CTRL5<4:2> MAG data rate selection
    uint8_t M_RES   :2;     // CTRL5<6:5> MAG resolution
    uint8_t TEMP_EN :1;     // CTRL5<7>   Temperature sensor enable
};

// control register 6 (LSM303D_REG_CTRL6 = 0x25)
struct lsm303d_reg_ctrl6 
{
    uint8_t unused1  :5;    // CTRL6<4:0> unused
    uint8_t MFS      :2;    // CTRL6<6:5> MAG full scale selection
    uint8_t unused2  :1;    // CTRL6<7>   unused
};

// control register 7 (LSM303D_REG_CTRL7 = 0x26)
struct lsm303d_reg_ctrl7
{
    uint8_t MD       :2;    // CTRL7<1:0> MAG sensor mode
    uint8_t MLP      :1;    // CTRL7<2>   MAG data low-power mode
    uint8_t unused   :1;    // CTRL7<3>   unused
    uint8_t T_ONLY   :1;    // CTRL7<4>   Temperature sensor only mode
    uint8_t AFDS     :1;    // CTRL7<5>   ACC data filtered
    uint8_t AHPM     :2;    // CTRL7<7:6> ACC HPF mode
};

// FIFO control (LSM303D_REG_FIFO_CTRL = 0x2e)
struct lsm303d_reg_fifo_ctrl
{
    uint8_t FTH  :5;   // FIFO_CTRL<4:0> FIFO threshold level
    uint8_t FM   :3;   // FIFO_CTRL<7:5> FIFO mode selection
};

// FIFO source (LSM303D_REG_FIFO_SRC = 0x2f)
struct lsm303d_reg_fifo_src
{
    uint8_t FFS  :5;   // FIFO_SRC<4:0> FIFO samples stored
    uint8_t EMPTY:1;   // FIFO_SRC<5>   FIFO is empty
    uint8_t OVRN :1;   // FIFO_SRC<6>   FIFO buffer full
    uint8_t FTH  :1;   // FIFO_SRC<7>   FIFO content exceeds watermark
};

// ACC interrupt generator IG_CFGx (LSM303D_REG_IG_CFGx = 0x30, 0x34)
struct lsm303d_reg_ig_cfgx
{
    uint8_t XLIE :1;   // IG_CFGx<0>    ACC x value below threshold enabled
    uint8_t XHIE :1;   // IG_CFGx<1>    ACC x value above threshold enabled
    uint8_t YLIE :1;   // IG_CFGx<2>    ACC y value below threshold enabled
    uint8_t YHIE :1;   // IG_CFGx<3>    ACC y value above threshold enabled
    uint8_t ZLIE :1;   // IG_CFGx<4>    ACC z value below threshold enabled
    uint8_t ZHIE :1;   // IG_CFGx<5>    ACC z value above threshold enabled
    uint8_t D6D  :1;   // IG_CFGx<6>    6D/4D detection detecetion enabled
    uint8_t AOI  :1;   // IG_CFGx<7>    AND/OR combination of interrupt events
};

// ACC interrupt source IG_SRCx (LSM303D_REG_IG_SRCx = 0x31, 0x35)
struct lsm303d_reg_ig_srcx
{
    uint8_t XL    :1;  // IG_SRCx<0>    ACC x value is below threshold 
    uint8_t XH    :1;  // IG_SRCx<1>    ACC x value is above threshold 
    uint8_t YL    :1;  // IG_SRCx<2>    ACC y value is below threshold 
    uint8_t YH    :1;  // IG_SRCx<3>    ACC y value is above threshold 
    uint8_t ZL    :1;  // IG_SRCx<4>    ACC z value is below threshold 
    uint8_t ZH    :1;  // IG_SRCx<5>    ACC z value is above threshold 
    uint8_t IA    :1;  // IG_SRCx<6>    ACC interrupt active
    uint8_t unused:1;  // IG_SRCx<7>    unused
};

// CLICK_CFGx (LSM303D_REG_CLICL_CFG = 0x38)
struct lsm303d_reg_click_cfg
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

static bool    lsm303d_reset       (lsm303d_sensor_t* dev);
static bool    lsm303d_is_available(lsm303d_sensor_t* dev);

static bool    lsm303d_i2c_read    (lsm303d_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len);
static bool    lsm303d_i2c_write   (lsm303d_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len);
static bool    lsm303d_spi_read    (lsm303d_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len);
static bool    lsm303d_spi_write   (lsm303d_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len);

#define msb_lsb_to_type(t,b,o) (t)(((t)b[o] << 8) | b[o+1])
#define lsb_msb_to_type(t,b,o) (t)(((t)b[o+1] << 8) | b[o])
#define lsb_to_type(t,b,o)     (t)(b[o])

#define lsm303d_update_reg(dev,addr,type,elem,value) \
        { \
            struct type __reg; \
            if (!lsm303d_reg_read (dev, (addr), (uint8_t*)&__reg, 1)) \
                return false; \
            __reg.elem = (value); \
            if (!lsm303d_reg_write (dev, (addr), (uint8_t*)&__reg, 1)) \
                return false; \
        }

lsm303d_sensor_t* lsm303d_init_sensor (uint8_t bus, uint8_t addr, uint8_t cs)
{
    lsm303d_sensor_t* dev;

    if ((dev = malloc (sizeof(lsm303d_sensor_t))) == NULL)
        return NULL;

    // init sensor data structure
    dev->bus    = bus;
    dev->addr   = addr;
    dev->cs     = cs;

    dev->error_code = LSM303D_OK;
    dev->a_scale    = lsm303d_a_scale_2_g;
    dev->m_scale    = lsm303d_m_scale_4_Gs;
    dev->m_res      = lsm303d_m_low_res;
    dev->fifo_mode  = lsm303d_bypass;
    dev->fifo_first = true;
    
    // if addr==0 then SPI is used and has to be initialized
    if (!addr && !spi_device_init (bus, cs))
    {
        error_dev ("Could not initialize SPI interface.", __FUNCTION__, dev);
        free (dev);
        return NULL;
    }
        
    // check availability of the sensor
    if (!lsm303d_is_available (dev))
    {
        error_dev ("Sensor is not available.", __FUNCTION__, dev);
        free (dev);
        return NULL;
    }

    // reset the sensor
    if (!lsm303d_reset(dev))
    {
        error_dev ("Could not reset the sensor device.", __FUNCTION__, dev);
        free (dev);
        return NULL;
    }
    
    // set block data update as default
    lsm303d_update_reg (dev, LSM303D_REG_CTRL1, lsm303d_reg_ctrl1, BDU, 1);

    // not necessary, following values are the defaults
    // lsm303d_update_reg (dev, LSM303D_REG_CTRL2, lsm303d_reg_ctrl2, AFS, lsm303d_a_scale_2_g);
    // lsm303d_update_reg (dev, LSM303D_REG_CTRL6, lsm303d_reg_ctrl6, MFS, lsm303d_m_scale_4_Gs);
    
    // clear FIFO
    // lsm303d_set_fifo_mode (sensor, lsm303d_bypass, 0);

    return dev;
}

bool lsm303d_set_a_mode (lsm303d_sensor_t* dev, 
                         lsm303d_a_odr_t odr, lsm303d_a_aaf_bw_t bw, 
                         bool x, bool y, bool z)
{
    if (!dev) return false;

    dev->error_code = LSM303D_OK;

    struct lsm303d_reg_ctrl1 ctrl1;
    struct lsm303d_reg_ctrl2 ctrl2;

    // read current register values
    if (!lsm303d_reg_read (dev, LSM303D_REG_CTRL1, (uint8_t*)&ctrl1, 1) ||
        !lsm303d_reg_read (dev, LSM303D_REG_CTRL2, (uint8_t*)&ctrl2, 1))
        return false;
   
    // set mode
    ctrl1.AXEN = x;
    ctrl1.AYEN = y;
    ctrl1.AZEN = z;
    ctrl1.AODR = odr;
    
    ctrl2.ABW  = bw;
    
    if (!lsm303d_reg_write (dev, LSM303D_REG_CTRL1, (uint8_t*)&ctrl1, 1) ||
        !lsm303d_reg_write (dev, LSM303D_REG_CTRL2, (uint8_t*)&ctrl2, 1))
        return false;
    
    // switching times
    vTaskDelay (50/portTICK_PERIOD_MS);

    return false;
}


bool lsm303d_set_m_mode (lsm303d_sensor_t* dev,
                         lsm303d_m_odr_t odr, 
                         lsm303d_m_resolution_t res, 
                         lsm303d_m_mode_t mode)
{
    if (!dev) return false;

    dev->error_code = LSM303D_OK;

    struct lsm303d_reg_ctrl5 ctrl5;
    struct lsm303d_reg_ctrl7 ctrl7;

    // read current register values
    if (!lsm303d_reg_read (dev, LSM303D_REG_CTRL5, (uint8_t*)&ctrl5, 1) ||
        !lsm303d_reg_read (dev, LSM303D_REG_CTRL7, (uint8_t*)&ctrl7, 1))
        return false;
   
    // set mode
    if (odr == lsm303d_m_low_power)
    {
        ctrl7.MLP = true;
        ctrl5.M_ODR = lsm303d_m_odr_3_125;
    }
    else
    {
        ctrl7.MLP = false;
        ctrl5.M_ODR = odr;
    }

    ctrl7.MD = mode;
    ctrl5.M_RES = res;
    
    // write register values
    if (!lsm303d_reg_write (dev, LSM303D_REG_CTRL5, (uint8_t*)&ctrl5, 1) ||
        !lsm303d_reg_write (dev, LSM303D_REG_CTRL7, (uint8_t*)&ctrl7, 1))
        return false;
    
    // switching times
    vTaskDelay (50/portTICK_PERIOD_MS);

    return false;
}


bool lsm303d_set_a_scale (lsm303d_sensor_t* dev, lsm303d_a_scale_t scale)
{
    if (!dev) return false;
    
    dev->error_code = LSM303D_OK;
    dev->a_scale = scale;
    
    // read CTRL2 register and write scale
    lsm303d_update_reg (dev, LSM303D_REG_CTRL2, lsm303d_reg_ctrl2, AFS, scale);
    
    return true;
}


bool lsm303d_set_m_scale (lsm303d_sensor_t* dev, lsm303d_m_scale_t scale)
{
    if (!dev) return false;
    
    dev->error_code = LSM303D_OK;
    dev->m_scale = scale;
    
    // read CTRL5 register and write scale
    lsm303d_update_reg (dev, LSM303D_REG_CTRL6, lsm303d_reg_ctrl6, MFS, scale);
    
    return true;
}


bool lsm303d_set_fifo_mode (lsm303d_sensor_t* dev, lsm303d_fifo_mode_t mode,
                            uint8_t thresh)
{
    if (!dev) return false;
    
    if (thresh > 31)
    {
        error_dev ("FIFO threshold is greate than the maximum of 31.", __FUNCTION__, dev);
        dev->error_code = LSM303D_FIFO_THRESHOLD_INVALID;
        return false;
    }
    
    dev->error_code = LSM303D_OK;
    dev->fifo_mode  = mode;
    
    // read CTRL1 register and write FIFO_EN flag
    lsm303d_update_reg (dev, LSM303D_REG_CTRL0, lsm303d_reg_ctrl0, FIFO_EN, mode != lsm303d_bypass);
    lsm303d_update_reg (dev, LSM303D_REG_CTRL0, lsm303d_reg_ctrl0, FTH_EN , mode != lsm303d_bypass);

    struct lsm303d_reg_fifo_ctrl fifo_ctrl = {
        .FTH = thresh,
        .FM  = mode,
    };

    // write FIFO_CTRL register
    if (!lsm303d_reg_write (dev, LSM303D_REG_FIFO_CTRL, (uint8_t*)&fifo_ctrl, 1))
        return false;

    return true;
}


bool lsm303d_new_a_data (lsm303d_sensor_t* dev)
{
    if (!dev) return false;

    dev->error_code = LSM303D_OK;

    if (dev->fifo_mode == lsm303d_bypass)
    {
        struct lsm303d_reg_status_a status;
        
        if (!lsm303d_reg_read (dev, LSM303D_REG_STATUS_A, (uint8_t*)&status, 1))
        {
            error_dev ("Could not get sensor status", __FUNCTION__, dev);
            return false;
        }
        return status.XADA || status.YADA || status.ZADA;
    }
    else
    {
        struct lsm303d_reg_fifo_src fifo_src;
        
        if (!lsm303d_reg_read (dev, LSM303D_REG_FIFO_SRC, (uint8_t*)&fifo_src, 1))
        {
            error_dev ("Could not get fifo source register data", __FUNCTION__, dev);
            return false;
        }
        return !fifo_src.EMPTY;
    }
}

bool lsm303d_new_m_data (lsm303d_sensor_t* dev)
{
    if (!dev) return false;

    dev->error_code = LSM303D_OK;

    struct lsm303d_reg_status_m status;
        
    if (!lsm303d_reg_read (dev, LSM303D_REG_STATUS_M, (uint8_t*)&status, 1))
    {
        error_dev ("Could not get sensor status", __FUNCTION__, dev);
        return false;
    }
    return status.XMDA || status.YMDA || status.ZMDA;
}


/**
 * Scaling factors for the conversion of raw sensor data to floating point g
 * values. Scaling factors are from mechanical characteristics in datasheet.
 *
 *  scale/sensitivity  resolution
 *       +-2 g         0.061 mg/LSB
 *       +-4 g         0.122 mg/LSB
 *       +-6 g         0.183 mg/LSB
 *       +-8 g         0.244 mg/LSB
 *      +-16 g         0.732 mg/LSB
 */
const static double  LSM303D_A_SCALES[5] = 
{ 0.061/1000, 0.122/1000, 0.183/1000, 0.244/1000, 0.732/1000 };

bool lsm303d_get_float_a_data (lsm303d_sensor_t* dev, lsm303d_float_a_data_t* data)
{
    if (!dev || !data) return false;

    lsm303d_raw_a_data_t raw;
    
    if (!lsm303d_get_raw_a_data (dev, &raw))
        return false;

    data->ax = LSM303D_A_SCALES[dev->a_scale] * raw.ax;
    data->ay = LSM303D_A_SCALES[dev->a_scale] * raw.ay;
    data->az = LSM303D_A_SCALES[dev->a_scale] * raw.az;

    return true;
}


uint8_t lsm303d_get_float_a_data_fifo (lsm303d_sensor_t* dev, lsm303d_float_a_data_fifo_t data)
{
    if (!dev || !data) return false;

    lsm303d_raw_a_data_fifo_t raw;
    
    uint8_t num = lsm303d_get_raw_a_data_fifo (dev, raw);

    for (int i = 0; i < num; i++)
    {
        data[i].ax = LSM303D_A_SCALES[dev->a_scale] * raw[i].ax;
        data[i].ay = LSM303D_A_SCALES[dev->a_scale] * raw[i].ay;
        data[i].az = LSM303D_A_SCALES[dev->a_scale] * raw[i].az;
    }
    return num;
}


/**
 * Scaling factors for the conversion of raw sensor data to floating point Gauss
 * values. Scaling factors are from sensor characteristics in datasheet.
 *
 *  scale/sensitivity  resolution
 *       +-2 Gauss     0.080 mGauss/LSB
 *       +-4 Gauss     0.160 mGauss/LSB
 *       +-8 Gauss     0.320 mGauss/LSB
 *      +-12 Gauss     0.479 mGauss/LSB
 */
const static double  LSM303D_M_SCALES[5] = { 0.080/1000, 0.160/1000, 0.320/1000, 0.479/1000 };

bool lsm303d_get_float_m_data (lsm303d_sensor_t* dev, lsm303d_float_m_data_t* data)
{
    if (!dev || !data) return false;

    lsm303d_raw_m_data_t raw;
    
    if (!lsm303d_get_raw_m_data (dev, &raw))
        return false;

    data->mx = LSM303D_M_SCALES[dev->m_scale] * raw.mx;
    data->my = LSM303D_M_SCALES[dev->m_scale] * raw.my;
    data->mz = LSM303D_M_SCALES[dev->m_scale] * raw.mz;

    return true;
}


bool lsm303d_get_raw_a_data (lsm303d_sensor_t* dev, lsm303d_raw_a_data_t* raw)
{
    if (!dev || !raw) return false;

    dev->error_code = LSM303D_OK;

    // abort if not in bypass mode
    if (dev->fifo_mode != lsm303d_bypass)
    {
        dev->error_code = LSM303D_SENSOR_IN_BYPASS_MODE;
        error_dev ("Sensor is in FIFO mode, use lsm303d_get_*_data_fifo to get data",
                   __FUNCTION__, dev);
        return false;
    }
    
    uint8_t buf[6];

    // read raw data sample
    if (!lsm303d_reg_read (dev, LSM303D_REG_OUT_X_L_A, buf, 6))
    {
        error_dev ("Could not get raw data sample", __FUNCTION__, dev);
        dev->error_code |= LSM303D_GET_RAW_A_DATA_FAILED;
        return false;
    }
    
    raw->ax = buf[1] << 8 | buf[0];
    raw->ay = buf[3] << 8 | buf[2];
    raw->az = buf[5] << 8 | buf[4];

    return true;
}


uint8_t lsm303d_get_raw_a_data_fifo (lsm303d_sensor_t* dev, lsm303d_raw_a_data_fifo_t raw)
{
    if (!dev) return 0;

    dev->error_code = LSM303D_OK;

    // in bypass mode, use lsm303d_get_raw_data to return one sample
    if (dev->fifo_mode == lsm303d_bypass)
        return lsm303d_get_raw_a_data (dev, raw) ? 1 : 0;
        
    struct lsm303d_reg_fifo_src fifo_src;
    
    // read FIFO state
    if (!lsm303d_reg_read (dev, LSM303D_REG_FIFO_SRC, (uint8_t*)&fifo_src, 1))
    {
        dev->error_code |= LSM303D_FIFO_GET_SRC_FAILED;
        error_dev ("Could not get fifo source register data", __FUNCTION__, dev);
        return 0;
    }

    // if nothing is in the FIFO, just return with 0
    if (fifo_src.EMPTY)
        return 0;

    uint8_t samples = fifo_src.FFS + (fifo_src.OVRN ? 1 : 0);
    uint8_t buf[6];

    // read samples from FIFO
    for (int i = 0; i < samples; i++)
    {
        if (!lsm303d_reg_read (dev, LSM303D_REG_OUT_X_L_A, buf, 6))
        {
            error_dev ("Could not get raw data samples", __FUNCTION__, dev);
            dev->error_code |= LSM303D_GET_RAW_A_DATA_FIFO_FAILED;
            return i;
        }
        
        raw[i].ax = buf[1] << 8 | buf[0];
        raw[i].ay = buf[3] << 8 | buf[2];
        raw[i].az = buf[5] << 8 | buf[4];
    }
    lsm303d_reg_read (dev, LSM303D_REG_FIFO_SRC, (uint8_t*)&fifo_src, 1);
    
    // if FFS is not 0 after all samples read, ODR is higher than fetching rate
    if (fifo_src.FFS)
    {
        dev->error_code = LSM303D_ODR_TOO_HIGH;
        error_dev ("New samples were stored in FIFO while reading, "
                   "output data rate (ODR) might be too high", __FUNCTION__, dev);
        return 0;
    }

    if (dev->fifo_mode == lsm303d_fifo && samples == 32)
    {
        // clean FIFO
        lsm303d_update_reg (dev, LSM303D_REG_FIFO_CTRL, lsm303d_reg_fifo_ctrl, FM, lsm303d_bypass);
        lsm303d_update_reg (dev, LSM303D_REG_FIFO_CTRL, lsm303d_reg_fifo_ctrl, FM, lsm303d_fifo);
    }

    return samples;
}


bool lsm303d_get_raw_m_data (lsm303d_sensor_t* dev, lsm303d_raw_m_data_t* raw)
{
    if (!dev || !raw) return false;

    dev->error_code = LSM303D_OK;

    uint8_t buf[6];

    // read raw data sample
    if (!lsm303d_reg_read (dev, LSM303D_REG_OUT_X_L_M, buf, 6))
    {
        error_dev ("Could not get raw data sample", __FUNCTION__, dev);
        dev->error_code |= LSM303D_GET_RAW_M_DATA_FAILED;
        return false;
    }
    
    raw->mx = buf[1] << 8 | buf[0];
    raw->my = buf[3] << 8 | buf[2];
    raw->mz = buf[5] << 8 | buf[4];

    return true;
}


bool lsm303d_enable_int (lsm303d_sensor_t* dev, 
                         lsm303d_int_type_t type,
                         lsm303d_int_signal_t signal, bool value)
{
    if (!dev) return false;

    dev->error_code = LSM303D_OK;

    struct lsm303d_reg_int_ctrl_m int_ctrl_m;
    struct lsm303d_reg_ctrl3      ctrl3;
    struct lsm303d_reg_ctrl4      ctrl4;
    
    uint8_t* reg1 = NULL;
    uint8_t* reg2 = NULL;
    uint8_t  addr1 = 0;
    uint8_t  addr2 = 0;

    // determine the addr of the register to change
    if (type == lsm303d_int_m_thresh)
    {
        reg1  = (uint8_t*)&int_ctrl_m;
        addr1 = LSM303D_REG_INT_CTRL_M;
        
        reg2  = (signal == lsm303d_int1_signal) ? (uint8_t*)&ctrl3 : (uint8_t*)&ctrl4;
        addr2 = (signal == lsm303d_int1_signal) ? LSM303D_REG_CTRL3 : LSM303D_REG_CTRL4;
    }
    else if (type == lsm303d_int_fifo_empty || signal == lsm303d_int1_signal)
    {
        reg1  = (uint8_t*)&ctrl3;
        addr1 = LSM303D_REG_CTRL3;
    }
    else
    {
        reg1  = (uint8_t*)&ctrl4;
        addr1 = LSM303D_REG_CTRL4;
    }

    // read the register
    if ((reg1 && !lsm303d_reg_read (dev, addr1, reg1, 1)) ||
        (reg2 && !lsm303d_reg_read (dev, addr2, reg2, 1)))
    {   
        error_dev ("Could not read interrupt control registers", __FUNCTION__, dev);
        dev->error_code |= LSM303D_INT_ENABLE_FAILED;
        return false;
    }

    // change the register
    switch (type)
    {
        case lsm303d_int_a_data_ready:  if (signal == lsm303d_int1_signal)
                                            ctrl3.INT1_DRDY_A = value;
                                        else
                                            ctrl4.INT2_DRDY_A = value;
                                        break;

        case lsm303d_int_m_data_ready:  if (signal == lsm303d_int1_signal)
                                            ctrl3.INT1_DRDY_M = value;
                                        else
                                            ctrl4.INT2_DRDY_M = value;
                                        break;
                
        case lsm303d_int_m_thresh:      int_ctrl_m.MIEN = value;
                                        if (signal == lsm303d_int1_signal)
                                            ctrl3.INT1_IGM = value;
                                        else
                                            ctrl4.INT2_IGM = value;
                                        break;
                
        case lsm303d_int_fifo_empty:    ctrl3.INT1_EMPTY = value;
                                        break;

        case lsm303d_int_fifo_thresh:   ctrl4.INT2_FTH = value;
                                        break;
                                        
        case lsm303d_int_fifo_overrun:  ctrl4.INT2_FTH = value;
                                        break;

        case lsm303d_int_event1:        if (signal == lsm303d_int1_signal)
                                            ctrl3.INT1_IG1 = value;
                                        else
                                            ctrl4.INT2_IG1 = value;
                                        break;

        case lsm303d_int_event2:        if (signal == lsm303d_int1_signal)
                                            ctrl3.INT1_IG2 = value;
                                        else
                                            ctrl4.INT2_IG2 = value;
                                        break;

        case lsm303d_int_click:         if (signal == lsm303d_int1_signal)
                                            ctrl3.INT1_Click = value;
                                        else
                                            ctrl4.INT2_Click = value;
                                        break;
                                        
        default:    error_dev ("Wrong interrupt type in enable function", __FUNCTION__, dev);
                    dev->error_code |= LSM303D_INT_TYPE_WRONG;
                    return false;
    }

    if ((reg1 && !lsm303d_reg_write (dev, addr1, reg1, 1)) ||
        (reg2 && !lsm303d_reg_write (dev, addr2, reg2, 1)))
    {   
        error_dev ("Could not enable/disable interrupt", __FUNCTION__, dev);
        dev->error_code |= LSM303D_INT_ENABLE_FAILED;
        return false;
    }
    
    return true;
}


bool lsm303d_get_int_data_source (lsm303d_sensor_t* dev,
                                  lsm303d_int_data_source_t* source)
{
    if (!dev || !source) return false;

    dev->error_code = LSM303D_OK;

    uint8_t status_a;
    uint8_t status_m;
    struct lsm303d_reg_fifo_src fifo_src;

    if (!lsm303d_reg_read (dev, LSM303D_REG_STATUS_A, &status_a, 1) ||
        !lsm303d_reg_read (dev, LSM303D_REG_STATUS_M, &status_m, 1) ||
        !lsm303d_reg_read (dev, LSM303D_REG_FIFO_SRC, (uint8_t*)&fifo_src, 1))
    {   
        error_dev ("Could not read source of interrupt INT1/INT2 from sensor", __FUNCTION__, dev);
        dev->error_code |= LSM303D_GET_INT_DATA_SOURCE_FAILED;
        return false;
    }

    source->a_data_ready = status_a & LSM303D_ANY_A_DATA_READY;
    source->m_data_ready = status_m & LSM303D_ANY_M_DATA_READY;

    source->fifo_empty   = fifo_src.EMPTY;
    source->fifo_thresh  = fifo_src.FTH;
    source->fifo_overrun = fifo_src.OVRN;

    return true;
}


bool lsm303d_set_int_m_thresh_config (lsm303d_sensor_t* dev,
                                      lsm303d_int_m_thresh_config_t* config)
{
    if (!dev || !config) return false;

    dev->error_code = LSM303D_OK;

    struct lsm303d_reg_int_ctrl_m int_ctrl_m;
    
    if (!lsm303d_reg_read (dev, LSM303D_REG_INT_CTRL_M, (uint8_t*)&int_ctrl_m, 1))
    {   
        error_dev ("Could not read configuration of magnetic threshold interrupt", __FUNCTION__, dev);
        dev->error_code |= LSM303D_SET_M_THRESH_CONFIG_FAILED;
        return false;
    }

    int_ctrl_m.XMIEN = config->x_enabled;
    int_ctrl_m.YMIEN = config->y_enabled;
    int_ctrl_m.ZMIEN = config->z_enabled;

    int_ctrl_m.MIEL  = config->latch;
    int_ctrl_m.MIEA  = config->signal_level;

    uint8_t int_ths_m [2] = { config->threshold & 0xff, config->threshold >> 8 };

    if (// write the threshold to registers INT_THS_*_M
        !lsm303d_reg_write (dev, LSM303D_REG_INT_THS_L_M, int_ths_m, 2) ||
        
        // write configuration to INT_CTRL_M
        !lsm303d_reg_write (dev, LSM303D_REG_INT_CTRL_M, (uint8_t*)&int_ctrl_m, 1))
    {   
        error_dev ("Could not configure magnetic threshold interrupt", __FUNCTION__, dev);
        dev->error_code |= LSM303D_SET_M_THRESH_CONFIG_FAILED;
        return false;
    }

    return true;
}


bool lsm303d_get_int_m_thresh_config (lsm303d_sensor_t* dev, 
                                      lsm303d_int_m_thresh_config_t* config)
{
    if (!dev || !config) return false;

    dev->error_code = LSM303D_OK;

    struct lsm303d_reg_int_ctrl_m int_ctrl_m;
    uint8_t int_ths_m [2];
    
    if (!lsm303d_reg_read (dev, LSM303D_REG_INT_THS_L_M, int_ths_m, 2) ||
        !lsm303d_reg_read (dev, LSM303D_REG_INT_CTRL_M , (uint8_t*)&int_ctrl_m, 1))
    {   
        error_dev ("Could not read configuration of magnetic threshold interrupt", __FUNCTION__, dev);
        dev->error_code |= LSM303D_GET_M_THRESH_CONFIG_FAILED;
        return false;
    }

    config->x_enabled    = int_ctrl_m.XMIEN;
    config->y_enabled    = int_ctrl_m.YMIEN;
    config->z_enabled    = int_ctrl_m.ZMIEN;

    config->latch        = int_ctrl_m.MIEL;
    config->signal_level = int_ctrl_m.MIEA;
    
    config->threshold    = int_ths_m[1] << 8 | int_ths_m[0];

    return true;
}


bool lsm303d_get_int_m_thresh_source (lsm303d_sensor_t* dev,
                                      lsm303d_int_m_thresh_source_t* source)
{
    if (!dev || !source) return false;

    dev->error_code = LSM303D_OK;

    struct lsm303d_reg_int_src_m  int_src_m;
    struct lsm303d_reg_int_ctrl_m int_ctrl_m;
    
    if (!lsm303d_reg_read (dev, LSM303D_REG_INT_SRC_M , (uint8_t*)&int_src_m , 1) ||
        !lsm303d_reg_read (dev, LSM303D_REG_INT_CTRL_M, (uint8_t*)&int_ctrl_m, 1))
    {   
        error_dev ("Could not read source of interrupt INT from sensor", __FUNCTION__, dev);
        dev->error_code |= LSM303D_GET_M_THRESH_SOURCE_FAILED;
        return false;
    }

    source->active = int_src_m.MINT;

    source->x_pos  = int_src_m.M_PTH_X & int_ctrl_m.XMIEN;
    source->x_neg  = int_src_m.M_NTH_X & int_ctrl_m.XMIEN;

    source->y_pos  = int_src_m.M_PTH_Y & int_ctrl_m.YMIEN;
    source->y_neg  = int_src_m.M_NTH_Y & int_ctrl_m.YMIEN;

    source->z_pos  = int_src_m.M_PTH_Z & int_ctrl_m.ZMIEN;
    source->z_neg  = int_src_m.M_NTH_Z & int_ctrl_m.ZMIEN;

    return true;
}


bool lsm303d_set_int_event_config (lsm303d_sensor_t* dev,
                                   lsm303d_int_event_config_t* config,
                                   lsm303d_int_event_gen_t gen)
{
    if (!dev || !config) return false;

    dev->error_code = LSM303D_OK;

    struct lsm303d_reg_ig_cfgx ig_cfgx;

    ig_cfgx.XLIE = config->x_low_enabled;
    ig_cfgx.XHIE = config->x_high_enabled;

    ig_cfgx.YLIE = config->y_low_enabled;
    ig_cfgx.YHIE = config->y_high_enabled;

    ig_cfgx.ZLIE = config->z_low_enabled;
    ig_cfgx.ZHIE = config->z_high_enabled;
    
    bool d4d_int = false;
    
    switch (config->mode)
    {
        case lsm303d_or          : ig_cfgx.AOI = 0; ig_cfgx.D6D = 0; break;
        case lsm303d_and         : ig_cfgx.AOI = 1; ig_cfgx.D6D = 0; break;

        case lsm303d_4d_movement : d4d_int = true;
        case lsm303d_6d_movement : ig_cfgx.AOI = 0; ig_cfgx.D6D = 1; break;

        case lsm303d_4d_position : d4d_int = true;
        case lsm303d_6d_position : ig_cfgx.AOI = 1; ig_cfgx.D6D = 1; break;
    }

    uint8_t ig_cfgx_addr = (gen == lsm303d_int_event1_gen) ? LSM303D_REG_IG_CFG1 : LSM303D_REG_IG_CFG2;
    uint8_t ig_thsx_addr = (gen == lsm303d_int_event1_gen) ? LSM303D_REG_IG_THS1 : LSM303D_REG_IG_THS2;
    uint8_t ig_durx_addr = (gen == lsm303d_int_event1_gen) ? LSM303D_REG_IG_DUR1 : LSM303D_REG_IG_DUR2;

    if (// write the thresholds to registers IG_THSx
        !lsm303d_reg_write (dev, ig_thsx_addr, &config->threshold, 1) ||
        
        // write duration configuration to IG_DURx 
        !lsm303d_reg_write (dev, ig_durx_addr, &config->duration, 1) ||
        
        // write configuration  to IG_CFGx
        !lsm303d_reg_write (dev, ig_cfgx_addr, (uint8_t*)&ig_cfgx, 1))
    {   
        error_dev ("Could not configure interrupt INT1", __FUNCTION__, dev);
        dev->error_code |= LSM303D_SET_EVENT_CONFIG_FAILED;
        return false;
    }
    
    if (gen == lsm303d_int_event1_gen)
    {
        lsm303d_update_reg (dev, LSM303D_REG_CTRL5, lsm303d_reg_ctrl5, LIR1, config->latch);
    }
    else
    {
        lsm303d_update_reg (dev, LSM303D_REG_CTRL5, lsm303d_reg_ctrl5, LIR2, config->latch);
    }
    lsm303d_update_reg (dev, LSM303D_REG_INT_CTRL_M, lsm303d_reg_int_ctrl_m, D4D, d4d_int);
        
    return true;
}


bool lsm303d_get_int_event_config (lsm303d_sensor_t* dev,
                                   lsm303d_int_event_config_t* config,
                                   lsm303d_int_event_gen_t gen)
{
    if (!dev || !config) return false;

    dev->error_code = LSM303D_OK;

    uint8_t ig_cfgx_addr = (gen == lsm303d_int_event1_gen) ? LSM303D_REG_IG_CFG1 : LSM303D_REG_IG_CFG2;
    uint8_t ig_thsx_addr = (gen == lsm303d_int_event1_gen) ? LSM303D_REG_IG_THS1 : LSM303D_REG_IG_THS2;
    uint8_t ig_durx_addr = (gen == lsm303d_int_event1_gen) ? LSM303D_REG_IG_DUR1 : LSM303D_REG_IG_DUR2;

    struct lsm303d_reg_int_ctrl_m int_ctrl_m;
    struct lsm303d_reg_ig_cfgx    ig_cfgx;
    struct lsm303d_reg_ctrl3      ctrl3;
    struct lsm303d_reg_ctrl5      ctrl5;

    if (!lsm303d_reg_read (dev, ig_cfgx_addr, (uint8_t*)&ig_cfgx, 1) ||
        !lsm303d_reg_read (dev, ig_thsx_addr, (uint8_t*)&config->threshold, 1) ||
        !lsm303d_reg_read (dev, ig_durx_addr, (uint8_t*)&config->duration, 1) ||
        !lsm303d_reg_read (dev, LSM303D_REG_INT_CTRL_M, (uint8_t*)&int_ctrl_m, 1) ||
        !lsm303d_reg_read (dev, LSM303D_REG_CTRL3, (uint8_t*)&ctrl3, 1) ||
        !lsm303d_reg_read (dev, LSM303D_REG_CTRL5, (uint8_t*)&ctrl5, 1))
    {   
        error_dev ("Could not read interrupt configuration from sensor", __FUNCTION__, dev);
        dev->error_code |= LSM303D_GET_EVENT_CONFIG_FAILED;
        return false;
    }

    config->x_low_enabled  = ig_cfgx.XLIE;
    config->x_high_enabled = ig_cfgx.XHIE;

    config->y_low_enabled  = ig_cfgx.YLIE;
    config->y_high_enabled = ig_cfgx.YHIE;

    config->z_low_enabled  = ig_cfgx.ZLIE;
    config->z_high_enabled = ig_cfgx.ZHIE;
    
    config->latch = (gen == lsm303d_int_event1_gen) ? ctrl5.LIR1 : ctrl5.LIR2;
    
    bool d4d_int = int_ctrl_m.D4D;
    
    if (ig_cfgx.AOI)
    {
        if (ig_cfgx.D6D && d4d_int)
            config->mode = lsm303d_4d_position;
        else if (ig_cfgx.D6D && !d4d_int)
            config->mode = lsm303d_6d_position;
        else
            config->mode = lsm303d_and;
    }
    else
    {
        if (ig_cfgx.D6D && d4d_int)
            config->mode = lsm303d_4d_movement;
        else if (ig_cfgx.D6D && !d4d_int)
            config->mode = lsm303d_6d_movement;
        else
            config->mode = lsm303d_or;
    }

    return true;
}


bool lsm303d_get_int_event_source (lsm303d_sensor_t* dev,
                                   lsm303d_int_event_source_t* source,
                                   lsm303d_int_event_gen_t gen)
{
    if (!dev || !source) return false;

    dev->error_code = LSM303D_OK;
    
    uint8_t ig_cfgx_addr = (gen == lsm303d_int_event1_gen) ? LSM303D_REG_IG_CFG1 : LSM303D_REG_IG_CFG2;
    uint8_t ig_srcx_addr = (gen == lsm303d_int_event1_gen) ? LSM303D_REG_IG_SRC1 : LSM303D_REG_IG_SRC2;

    struct lsm303d_reg_ig_cfgx ig_cfgx;
    struct lsm303d_reg_ig_srcx ig_srcx;

    if (!lsm303d_reg_read (dev, ig_srcx_addr, (uint8_t*)&ig_srcx, 1) ||
        !lsm303d_reg_read (dev, ig_cfgx_addr, (uint8_t*)&ig_cfgx, 1))
    {   
        error_dev ("Could not read source of interrupt INT1/INT2 from sensor", __FUNCTION__, dev);
        dev->error_code |= LSM303D_GET_EVENT_SOURCE_FAILED;
        return false;
    }


    source->active = ig_srcx.IA;
    source->x_low  = ig_srcx.XL & ig_cfgx.XLIE;
    source->x_high = ig_srcx.XH & ig_cfgx.XHIE;
    source->y_low  = ig_srcx.YL & ig_cfgx.YLIE;
    source->y_high = ig_srcx.YH & ig_cfgx.YHIE;
    source->z_low  = ig_srcx.ZL & ig_cfgx.ZLIE;
    source->z_high = ig_srcx.ZH & ig_cfgx.ZHIE;
    
    return true;
}


bool lsm303d_set_int_click_config (lsm303d_sensor_t* dev,
                                   lsm303d_int_click_config_t* config)
{
    if (!dev || !config) return false;

    dev->error_code = LSM303D_OK;

    struct lsm303d_reg_click_cfg  click_cfg;

    click_cfg.XS = config->x_single;
    click_cfg.XD = config->x_double;

    click_cfg.YS = config->y_single;
    click_cfg.YD = config->y_double;

    click_cfg.ZS = config->z_single;
    click_cfg.ZD = config->z_double;
    
    uint8_t click_ths = config->threshold | ((config->latch) ? 0x80 : 0x00);

    if (!lsm303d_reg_write (dev, LSM303D_REG_CLICK_CFG   , (uint8_t*)&click_cfg, 1) ||
        !lsm303d_reg_write (dev, LSM303D_REG_CLICK_THS   , (uint8_t*)&click_ths, 1) ||
        !lsm303d_reg_write (dev, LSM303D_REG_TIME_LIMIT  , (uint8_t*)&config->time_limit, 1) ||
        !lsm303d_reg_write (dev, LSM303D_REG_TIME_LATENCY, (uint8_t*)&config->time_latency, 1) ||
        !lsm303d_reg_write (dev, LSM303D_REG_TIME_WINDOW , (uint8_t*)&config->time_window, 1))
    {   
        error_dev ("Could not configure click detection interrupt", __FUNCTION__, dev);
        dev->error_code |= LSM303D_SET_CLICK_CONFIG_FAILED;
        return false;
    }
    
    return true;
}

bool lsm303d_get_int_click_config (lsm303d_sensor_t* dev,
                                   lsm303d_int_click_config_t* config)
{
    if (!dev || !config) return false;

    dev->error_code = LSM303D_OK;

    struct lsm303d_reg_click_cfg  click_cfg;
    uint8_t click_ths;

    if (!lsm303d_reg_read (dev, LSM303D_REG_CLICK_CFG   , (uint8_t*)&click_cfg, 1) ||
        !lsm303d_reg_read (dev, LSM303D_REG_CLICK_THS   , (uint8_t*)&click_ths, 1) ||
        !lsm303d_reg_read (dev, LSM303D_REG_TIME_LIMIT  , (uint8_t*)&config->time_limit, 1) ||
        !lsm303d_reg_read (dev, LSM303D_REG_TIME_LATENCY, (uint8_t*)&config->time_latency, 1) ||
        !lsm303d_reg_read (dev, LSM303D_REG_TIME_WINDOW , (uint8_t*)&config->time_window, 1))
    {   
        error_dev ("Could not configure click detection interrupt", __FUNCTION__, dev);
        dev->error_code |= LSM303D_GET_CLICK_CONFIG_FAILED;
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

bool lsm303d_get_int_click_source (lsm303d_sensor_t* dev,
                                   lsm303d_int_click_source_t* source)
{
    if (!dev || !source) return false;

    dev->error_code = LSM303D_OK;

    if (!lsm303d_reg_read (dev, LSM303D_REG_CLICK_SRC, (uint8_t*)source, 1))
    {   
        error_dev ("Could not read source of click interrupt from sensor", __FUNCTION__, dev);
        dev->error_code |= LSM303D_GET_CLICK_SOURCE_FAILED;
        return false;
    }

    return true;
}
                                     


bool lsm303d_config_int_signals (lsm303d_sensor_t* dev,
                                 lsm303d_int_signal_type_t type)
{
    if (!dev) return false;

    dev->error_code = LSM303D_OK;

    lsm303d_update_reg (dev, LSM303D_REG_INT_CTRL_M, lsm303d_reg_int_ctrl_m, PP_OD, type);
    
    return true;
}


bool lsm303d_config_a_hpf (lsm303d_sensor_t* dev, 
                           lsm303d_hpf_mode_t mode,
                           bool data, bool click, bool int1, bool int2)
{
    if (!dev) return false;

    dev->error_code = LSM303D_OK;

    lsm303d_update_reg (dev, LSM303D_REG_CTRL7, lsm303d_reg_ctrl7, AHPM    , mode);
    lsm303d_update_reg (dev, LSM303D_REG_CTRL7, lsm303d_reg_ctrl7, AFDS    , data);
    lsm303d_update_reg (dev, LSM303D_REG_CTRL0, lsm303d_reg_ctrl0, HP_Click, click);
    lsm303d_update_reg (dev, LSM303D_REG_CTRL0, lsm303d_reg_ctrl0, HPIS1   , int1);
    lsm303d_update_reg (dev, LSM303D_REG_CTRL0, lsm303d_reg_ctrl0, HPIS2   , int2);

    int8_t x_ref;
    int8_t y_ref;
    int8_t z_ref;
    
    if (mode == lsm303d_hpf_normal)
        lsm303d_get_a_hpf_ref (dev, &x_ref, &y_ref, &z_ref);

    return true;
}


bool lsm303d_set_a_hpf_ref (lsm303d_sensor_t* dev, 
                            int8_t x_ref, int8_t y_ref, int8_t z_ref)
{
    if (!dev) return false;

    dev->error_code = LSM303D_OK;

    if (!lsm303d_reg_write (dev, LSM303D_REG_REFERENCE_X, (uint8_t*)&x_ref, 1) ||
        !lsm303d_reg_write (dev, LSM303D_REG_REFERENCE_Y, (uint8_t*)&y_ref, 1) ||
        !lsm303d_reg_write (dev, LSM303D_REG_REFERENCE_Z, (uint8_t*)&z_ref, 1))
    {   
        error_dev ("Could not set high pass filter reference", __FUNCTION__, dev);
        dev->error_code |= LSM303D_SET_HPF_REF_FAILED;
        return false;
    }

    return true;
}


bool lsm303d_get_a_hpf_ref (lsm303d_sensor_t* dev, 
                            int8_t* x_ref, int8_t* y_ref, int8_t* z_ref)
{
    if (!dev) return false;

    dev->error_code = LSM303D_OK;

    if (!lsm303d_reg_read (dev, LSM303D_REG_REFERENCE_X, (uint8_t*)x_ref, 1) ||
        !lsm303d_reg_read (dev, LSM303D_REG_REFERENCE_Y, (uint8_t*)y_ref, 1) ||
        !lsm303d_reg_read (dev, LSM303D_REG_REFERENCE_Z, (uint8_t*)z_ref, 1))
    {   
        error_dev ("Could not get high pass filter reference", __FUNCTION__, dev);
        dev->error_code |= LSM303D_GET_HPF_REF_FAILED;
        return false;
    }

    return true;
}


bool lsm303d_set_m_offset (lsm303d_sensor_t* dev, 
                           int16_t x_off, int16_t y_off, int16_t z_off)
{
    if (!dev) return false;

    dev->error_code = LSM303D_OK;
    
    uint8_t buf [6] = { 
                        x_off & 0xff, x_off >> 8,
                        y_off & 0xff, y_off >> 8,
                        z_off & 0xff, z_off >> 8
                     };

    if (!lsm303d_reg_write (dev, LSM303D_REG_OFFSET_X_L_M, buf, 6))
    {   
        error_dev ("Could not set magnetic offset", __FUNCTION__, dev);
        dev->error_code |= LSM303D_SET_M_OFFSET_FAILED;
        return false;
    }

    return true;
}


bool lsm303d_get_m_offset (lsm303d_sensor_t* dev, 
                           int16_t* x_off, int16_t* y_off, int16_t* z_off)
{
    if (!dev) return false;

    dev->error_code = LSM303D_OK;

    uint8_t buf [6];

    if (!lsm303d_reg_read (dev, LSM303D_REG_OFFSET_X_L_M, buf, 6))
    {   
        error_dev ("Could not get magnetic offset", __FUNCTION__, dev);
        dev->error_code |= LSM303D_GET_M_OFFSET_FAILED;
        return false;
    }

    *x_off = buf[1] << 8 | buf[0];
    *y_off = buf[3] << 8 | buf[2];
    *z_off = buf[5] << 8 | buf[4];

    return true;
}


bool lsm303d_enable_temperature (lsm303d_sensor_t* dev, bool enable)
{
    lsm303d_update_reg (dev, LSM303D_REG_CTRL5, lsm303d_reg_ctrl5, TEMP_EN, enable);
    
    return true;
}


float lsm303d_get_temperature (lsm303d_sensor_t* dev)
{
    uint8_t regs[2];

    // read raw data sample
    if (!lsm303d_reg_read (dev, LSM303D_REG_TEMP_OUT_L, regs, 2))
    {
        error_dev ("Could not get temperature data sample", __FUNCTION__, dev);
        dev->error_code |= LSM303D_GET_RAW_T_DATA_FAILED;
        return false;
    }
    
    return (int16_t)(regs[1] << 8 | regs[0]) / 8.0 + 25.0;
}


/** Functions for internal use only */

/**
 * @brief   Check the chip ID to test whether sensor is available
 */
static bool lsm303d_is_available (lsm303d_sensor_t* dev)
{
    uint8_t chip_id;

    if (!dev) return false;

    dev->error_code = LSM303D_OK;

    if (!lsm303d_reg_read (dev, LSM303D_REG_WHO_AM_I, &chip_id, 1))
        return false;

    if (chip_id != LSM303D_CHIP_ID)
    {
        error_dev ("Chip id %02x is wrong, should be %02x.",
                    __FUNCTION__, dev, chip_id, LSM303D_CHIP_ID);
        dev->error_code = LSM303D_WRONG_CHIP_ID;
        return false;
    }

    return true;
}

static bool lsm303d_reset (lsm303d_sensor_t* dev)
{
    if (!dev) return false;

    dev->error_code = LSM303D_OK;

    uint8_t int_ctrl_m    = 0x08; // 0xe8
    uint8_t ctrl_regs[]   = { 0x00, 0x00 /*0x07*/, 0x00, 0x00, 0x00, 0x18, 0x20, 0x01 };
    uint8_t null_regs[11] = { 0 };
    
    // initialize sensor completely including setting in power down mode
    lsm303d_reg_write (dev, LSM303D_REG_INT_CTRL_M , &int_ctrl_m, 1 );
    lsm303d_reg_write (dev, LSM303D_REG_INT_THS_L_M, null_regs  , 11);
    lsm303d_reg_write (dev, LSM303D_REG_CTRL0      , ctrl_regs  , 8 );
    lsm303d_reg_write (dev, LSM303D_REG_FIFO_CTRL  , null_regs  , 1 );
    lsm303d_reg_write (dev, LSM303D_REG_IG_CFG1    , null_regs  , 1 );
    lsm303d_reg_write (dev, LSM303D_REG_IG_THS1    , null_regs  , 2 );
    lsm303d_reg_write (dev, LSM303D_REG_IG_CFG2    , null_regs  , 1 );
    lsm303d_reg_write (dev, LSM303D_REG_IG_THS2    , null_regs  , 2 );
    lsm303d_reg_write (dev, LSM303D_REG_CLICK_CFG  , null_regs  , 1 );
    lsm303d_reg_write (dev, LSM303D_REG_CLICK_THS  , null_regs  , 4 );
    
    return true;
}


bool lsm303d_reg_read(lsm303d_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len)
{
    if (!dev || !data) return false;

    return (dev->addr) ? lsm303d_i2c_read (dev, reg, data, len)
                       : lsm303d_spi_read (dev, reg, data, len);
}


bool lsm303d_reg_write(lsm303d_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len)
{
    if (!dev || !data) return false;

    return (dev->addr) ? lsm303d_i2c_write (dev, reg, data, len)
                       : lsm303d_spi_write (dev, reg, data, len);
}


#define LSM303D_SPI_BUF_SIZE 64      // SPI register data buffer size of ESP866

#define LSM303D_SPI_READ_FLAG      0x80
#define LSM303D_SPI_WRITE_FLAG     0x00
#define LSM303D_SPI_AUTO_INC_FLAG  0x40

static bool lsm303d_spi_read(lsm303d_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len)
{
    if (!dev || !data) return false;

    if (len >= LSM303D_SPI_BUF_SIZE)
    {
        dev->error_code |= LSM303D_SPI_BUFFER_OVERFLOW;
        error_dev ("Error on read from SPI slave on bus 1. Tried to transfer "
                   "more than %d byte in one read operation.",
                   __FUNCTION__, dev, LSM303D_SPI_BUF_SIZE);
        return false;
    }

    uint8_t addr = (reg & 0x3f) | LSM303D_SPI_READ_FLAG | LSM303D_SPI_AUTO_INC_FLAG;
    
    static uint8_t mosi[LSM303D_SPI_BUF_SIZE];
    static uint8_t miso[LSM303D_SPI_BUF_SIZE];

    memset (mosi, 0xff, LSM303D_SPI_BUF_SIZE);
    memset (miso, 0xff, LSM303D_SPI_BUF_SIZE);

    mosi[0] = addr;
    
    if (!spi_transfer_pf (dev->bus, dev->cs, mosi, miso, len+1))
    {
        error_dev ("Could not read data from SPI", __FUNCTION__, dev);
        dev->error_code |= LSM303D_SPI_READ_FAILED;
        return false;
    }
    
    // shift data one by left, first byte received while sending register address is invalid
    for (int i=0; i < len; i++)
      data[i] = miso[i+1];

    #ifdef LSM303D_DEBUG_LEVEL_2
    printf("LSM303D %s: read the following bytes from reg %02x: ", __FUNCTION__, reg);
    for (int i=0; i < len; i++)
        printf("%02x ", data[i]);
    printf("\n");
    #endif

    return true;
}


static bool lsm303d_spi_write(lsm303d_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len)
{
    if (!dev || !data) return false;

    uint8_t addr = (reg & 0x3f) | LSM303D_SPI_WRITE_FLAG | LSM303D_SPI_AUTO_INC_FLAG;

    static uint8_t mosi[LSM303D_SPI_BUF_SIZE];

    if (len >= LSM303D_SPI_BUF_SIZE)
    {
        dev->error_code |= LSM303D_SPI_BUFFER_OVERFLOW;
        error_dev ("Error on write to SPI slave on bus 1. Tried to transfer more"
                   "than %d byte in one write operation.", 
                   __FUNCTION__, dev, LSM303D_SPI_BUF_SIZE);

        return false;
    }

    reg &= 0x7f;

    // first byte in output is the register address
    mosi[0] = addr;

    // shift data one byte right, first byte in output is the register address
    for (int i = 0; i < len; i++)
        mosi[i+1] = data[i];

    #ifdef LSM303D_DEBUG_LEVEL_2
    printf("LSM303D %s: Write the following bytes to reg %02x: ", __FUNCTION__, reg);
    for (int i = 1; i < len+1; i++)
        printf("%02x ", mosi[i]);
    printf("\n");
    #endif

    if (!spi_transfer_pf (dev->bus, dev->cs, mosi, NULL, len+1))
    {
        error_dev ("Could not write data to SPI.", __FUNCTION__, dev);
        dev->error_code |= LSM303D_SPI_WRITE_FAILED;
        return false;
    }

    return true;
}


#define I2C_AUTO_INCREMENT (0x80)

static bool lsm303d_i2c_read(lsm303d_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len)
{
    if (!dev || !data) return false;

    debug_dev ("Read %d byte from i2c slave register %02x.", __FUNCTION__, dev, len, reg);

    if (len > 1)
        reg |= I2C_AUTO_INCREMENT;
    
    int result = i2c_slave_read(dev->bus, dev->addr, &reg, data, len);

    if (result)
    {
        dev->error_code |= (result == -EBUSY) ? LSM303D_I2C_BUSY : LSM303D_I2C_READ_FAILED;
        error_dev ("Error %d on read %d byte from I2C slave register %02x.",
                    __FUNCTION__, dev, result, len, reg);
        return false;
    }

#   ifdef LSM303D_DEBUG_LEVEL_2
    printf("LSM303D %s: Read following bytes: ", __FUNCTION__);
    printf("%02x: ", reg & 0x7f);
    for (int i=0; i < len; i++)
        printf("%02x ", data[i]);
    printf("\n");
#   endif

    return true;
}


static bool lsm303d_i2c_write(lsm303d_sensor_t* dev, uint8_t reg, uint8_t *data, uint16_t len)
{
    if (!dev || !data) return false;

    debug_dev ("Write %d byte to i2c slave register %02x.", __FUNCTION__, dev, len, reg);

    if (len > 1)
        reg |= I2C_AUTO_INCREMENT;

    int result = i2c_slave_write(dev->bus, dev->addr, &reg, data, len);

    if (result)
    {
        dev->error_code |= (result == -EBUSY) ? LSM303D_I2C_BUSY : LSM303D_I2C_WRITE_FAILED;
        error_dev ("Error %d on write %d byte to i2c slave register %02x.",
                    __FUNCTION__, dev, result, len, reg);
        return false;
    }

#   ifdef LSM303D_DEBUG_LEVEL_2
    printf("LSM303D %s: Wrote the following bytes: ", __FUNCTION__);
    printf("%02x: ", reg & 0x7f);
    for (int i=0; i < len; i++)
        printf("%02x ", data[i]);
    printf("\n");
#   endif

    return true;
}
