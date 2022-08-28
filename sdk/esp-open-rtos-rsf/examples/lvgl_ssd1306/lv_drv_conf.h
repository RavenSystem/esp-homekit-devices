/**
 * @file lv_drv_conf.h
 *
 */

#if 1 /*Remove this to enable the content*/

#ifndef LV_DRV_CONF_H
#define LV_DRV_CONF_H

#include "lv_conf.h"

/*********************
 *      INCLUDES
 *********************/
/* Add specific sdk include here */
#include <espressif/esp_common.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include <i2c/i2c.h>
#include <esp/gpio.h>
#include <esp/spi.h>
/*********************
 *       DEFINES
 *********************/
/* Disable with 0 if driver don't use an specific api */
#define LV_DRIVER_ENABLE_COMMON 1
#define LV_DRIVER_ENABLE_DELAY 1
#define LV_DRIVER_ENABLE_I2C 1
#define LV_DRIVER_ENABLE_SPI 1
#define LV_DRIVER_ENABLE_PAR 1

/* use this macro if you want ignore a gpio write/read. e.g: spi.cs = LV_DRIVER_NOPIN */
#define LV_DRIVER_NOPIN (0xFF)

/* use this macro to add specific attribute to function call into an interupt routines */
#define INTERUPT_ATTRIBUTE __attribute__((section(".iram1.text")))

/**********************
 *      TYPEDEFS
 **********************/
/* You can use a pointer handler or just a id number to reder to a device
 * e.g: typedef const uint8_t lv_gpio_handle_t if you need just a bus id
 * You can use device descriptor from your sdk too.
 */
//typedef const void* lv_gpio_handle_t ;
//typedef const void* lv_i2c_handle_t ;
//typedef const void* lv_spi_handle_t ;
typedef const void* lv_par_handle_t ;
typedef const void* lv_uart_handle_t ;

typedef struct {
    uint8_t bus;
    uint8_t cs;
    uint8_t dc;
    spi_settings_t *s; //XXX:experimental for multi spi device
    SemaphoreHandle_t* mutex; //XXX:experimental for multi spi device
} lv_spi_dev_t, *lv_spi_handle_t;

typedef const i2c_dev_t *lv_i2c_handle_t ;
typedef uint8_t lv_gpio_handle_t ;

/*********************
 * 	HAL INTERFACE
 *********************/
/*
 * All used peripherals must be initialized in user application, library only
 * manipulate them.
 * You can use a device descriptor from your SDK or  do your own in this file too.
 * example:
 * typedef struct lv_spi_dev_t { .... };
 */

/*------------
 *  Delay
 *------------*/
#if LV_DRIVER_ENABLE_DELAY

/**
 * Delay the given number of microseconds
 * @param us Time to wait in us
 */
static inline void lv_delay_us(const uint32_t us)
{
   	sdk_os_delay_us(us);
}

/**
 * Delay the given number of milliseconds
 * @param ms Time to wait in ms
 */
static inline void lv_delay_ms(const uint32_t ms)
{
    if (ms/portTICK_PERIOD_MS) //if we wait more than a tick, use vTaskDelay
    {
        vTaskDelay(((ms-1)/portTICK_PERIOD_MS)+1); //Round superior
        return;
    }
	for (uint32_t x = 0 ; x < ms ; x++)
	{
		sdk_os_delay_us(1000);
	}
}

/**
 * Return system time
 * @return system time (ms)
 */
static inline uint32_t lv_get_ms()
{
    return xTaskGetTickCount()*portTICK_PERIOD_MS;
}

#endif
/*------------
 *  Common
 *------------*/
#if LV_DRIVER_ENABLE_COMMON

/**
 * Change a pin level
 * @param pin gpio Number
 * @param val Level to set
 */
static inline void lv_gpio_write(lv_gpio_handle_t gpio, const uint8_t val)
{
    gpio_write(gpio, val);
}

/**
 * Read current level gpio
 * @param pin gpio to read
 * @return gpio value
 */
static inline uint8_t lv_gpio_read(lv_gpio_handle_t gpio)
{
	return gpio_read(gpio);
}

#endif

/*---------
 *  UART
 *---------*/
/**
 * Do a uart write transmission.
 * @param uart_dev Pointer to uart device
 * @param data_out Pointer to data buffer to send if non-null
 * @param datalen Number of data byte to send
 * @return Non-Zero if error occured
 */
static inline int lv_uart_write(lv_uart_handle_t uart_dev, const void* data_out, uint16_t datalen)
{
    return -1;
}

/**
 * Do a uart read transmission
 * @param uart_dev Pointer to uart device
 * @param data_out Pointer to data buffer to send if non-null
 * @param datalen Number of data byte to read
 * @return Non-Zero if error occured
 */
static inline int lv_uart_read(lv_uart_handle_t uart_dev, void* data_in, uint16_t datalen)
{
    return -1;
}

/*---------
 *  I2C
 *---------*/
#if LV_DRIVER_ENABLE_I2C

/**
 * Do a I2C write transmission on 8 bits register device.
 * @param i2c_dev Pointer to i2c device
 * @param reg Pointer to register address to send if non-null
 * @param data_out Pointer to data buffer to send if non-null
 * @param datalen Number of data byte to send
 * @return Non-Zero if error occured
 */
static inline int lv_i2c_write(lv_i2c_handle_t i2c_dev, const uint8_t* reg, const void* data_out, uint16_t datalen)
{
    return i2c_slave_write(i2c_dev->bus, i2c_dev->addr, reg, (uint8_t*)data_out, datalen);
}

/**
 * Do a I2C read transmission on 8 bits register device.
 * @param i2c_dev Pointer to i2c device
 * @param reg Pointer to register address to send if non-null
 * @param data_out Pointer to data buffer to send if non-null
 * @param datalen Number of data byte to send
 * @return Non-Zero if error occured
 */
static inline int lv_i2c_read(lv_i2c_handle_t i2c_dev, const uint8_t* reg, void* data_in, uint16_t datalen)
{
    return i2c_slave_read(i2c_dev->bus, i2c_dev->addr, reg, (uint8_t*)data_in, datalen);
}

/**
 * Do a I2C write transmissionon 16 bits register device
 * @param i2c_dev Pointer to i2c device
 * @param reg Pointer to register address to send if non-null
 * @param data_out Pointer to data buffer to send if non-null
 * @param datalen Number of data byte to send
 * @return Non-Zero if error occured
 */
static inline int lv_i2c_write16(lv_i2c_handle_t i2c_dev, const uint16_t* reg, const void* data_out, uint16_t datalen)
{
    return -1;
}

/**
 * Do a I2C write transmissionon 16 bits register device.
 * @param i2c_dev Pointer to i2c device
 * @param reg Pointer to register address to send if non-null
 * @param data_out Pointer to data buffer to send if non-null
 * @param datalen Number of data byte to send
 * @return Non-Zero if error occured
 */
static inline int lv_i2c_read16(lv_i2c_handle_t i2c_dev, const uint16_t* reg, void* data_in, uint16_t datalen)
{
    return -1;
}

#endif
/*---------
 *  SPI
 *---------*/
#if LV_DRIVER_ENABLE_SPI

typedef enum {
  LV_SPI_COMMAND,
  LV_SPI_ADDRESS,
  LV_SPI_DUMMY,
} lv_spi_reg_t;

/**
 * Control SPI cs pin.
 * @param spi_dev Pointer to spi device
 * @param lvl Gpio Level
 */
static inline void lv_spi_wr_cs(lv_spi_handle_t spi_dev, uint8_t lvl)
{
    gpio_write(spi_dev->cs, lvl);
}

/**
 * Control SPI dc pin.
 * @param spi_dev Pointer to spi device
 * @param lvl Gpio Level
 */
static inline void lv_spi_wr_dc(lv_spi_handle_t spi_dev, uint8_t lvl)
{
    gpio_write(spi_dev->dc, lvl);
}

/**
 * Do a SPI transaction .
 * @param spi_dev Pointer to spi device
 * @param data_in Receive buffer. If NULL, received data will be lost.
 * @param data_out Data to send buffer. If NULL, it will only receive data.
 * @param len Buffer size in words
 * @param word_size Size of the word in byte
 * @return Non-Zero if error occured
 */
static inline int lv_spi_transaction(lv_spi_handle_t spi_dev, void* data_in, const void* data_out, uint16_t len, uint8_t word_size)
{
    spi_transfer(spi_dev->bus, data_out, data_in, len, word_size);
	return 0;
}

/**
 * Do a SPI repeat send.
 * @param spi_dev Pointer to spi device
 * @param template Pointer toTemplate to send throw spi.
 * @param repeats Copy number
 * @param template_size Size of the template in byte
 * @return Non-Zero if error occured
 */
static inline int lv_spi_repeat(lv_spi_handle_t spi_dev, const void* template, uint32_t repeats, uint8_t template_size)
{
	uint32_t data = 0;
	if(!template)
	{
		template = (void*)&data ;
	}
	switch (template_size)
	{
	case 1:
		spi_repeat_send_8(spi_dev->bus,*(uint8_t*)template,repeats);
		break;
	case 2:
        spi_repeat_send_16(spi_dev->bus,*(uint16_t*)template,repeats);
		break;
	case 4:
		spi_repeat_send_32(spi_dev->bus,*(uint32_t*)template,repeats);
		break;
	default:
		return -1;
		break;
	}
	return 0;
}

/**
 * Set command to send for spi transaction
 * @param spi_dev Pointer to spi device
 * @param reg SPI register to set (dummy/command/address)
 * @param value Value
 * @param bits Bits number
 * @return Non-Zero if error occured
 */
static inline int lv_spi_set_preemble(lv_spi_handle_t spi_dev, lv_spi_reg_t reg, uint32_t value, uint8_t bits)
{
    switch(reg) {
        case LV_SPI_COMMAND:
            spi_set_command(spi_dev->bus, bits, value);
            break;
        case LV_SPI_ADDRESS:
            spi_set_address(spi_dev->bus, bits, value);
            break;
        case LV_SPI_DUMMY:
            spi_set_dummy_bits(spi_dev->bus,  bits, false);
            break;
        default:
            return -1;
            break;
    }
	return 0;
}

/**
 * Clear spi bus command
 * @param spi_dev Pointer to spi device
 * @param reg SPI register to clear (dummy/command/address)
 * @return Non-Zero if error occured
 */
static inline int lv_spi_clr_preemble(lv_spi_handle_t spi_dev, lv_spi_reg_t reg)
{
    switch(reg) {
        case LV_SPI_COMMAND:
            spi_clear_command(spi_dev->bus);
            break;
        case LV_SPI_ADDRESS:
            spi_clear_address(spi_dev->bus);
            break;
        case LV_SPI_DUMMY:
            spi_clear_dummy(spi_dev->bus);
            break;
        default:
            return -1;
            break;
    }
	return 0;
}

#endif
/*------------------
 *  Parallel port
 *-----------------*/
#if LV_DRIVER_ENABLE_PAR
/**
 * Control Parallel cs pin.
 * @param par_dev Pointer to parallel device
 * @param lvl Gpio Level
 */
static inline void lv_par_wr_cs(lv_par_handle_t par_dev, uint8_t lvl)
{
    return;
}

/**
 * Control Parallel dc pin.
 * @param par_dev Pointer to parallel device
 * @param lvl Gpio Level
 */
static inline void lv_par_wr_dc(lv_par_handle_t par_dev, uint8_t lvl)
{
    return;
}

/**
 * Do a Parallel port write.
 * @param par_dev Pointer to parallel port device
 * @param data_out Pointer to data buffer to send
 * @param len Buffer size in words
 * @param word_size Size of the word in byte
 * @return Non-Zero if error occured
 */
static inline int lv_par_write(lv_par_handle_t par_dev, const void* data_out, uint16_t len, uint8_t word_size)
{
    return -1;
}

/**
 * Do a Parallel port read.
 * @param par_dev Pointer to parallel port device
 * @param data_in Pointer to data buffer to read
 * @param len Buffer size in words
 * @param word_size Size of the word in byte
 * @return Non-Zero if error occured
 */
static inline int lv_par_read(lv_par_handle_t par_dev, void* data_in, uint16_t len, uint8_t word_size)
{
  return -1;
}

#endif
/*********************
 *  DISPLAY DRIVERS
 *********************/

/*-------------------
 *  Monitor of PC
 *-------------------*/
#define USE_MONITOR         0
#if USE_MONITOR
#define MONITOR_HOR_RES     LV_HOR_RES
#define MONITOR_VER_RES     LV_HOR_VER
#endif

/*----------------
 *    SSD1963
 *--------------*/
#define USE_SSD1963         0
#if USE_SSD1963
#define SSD1963_HOR_RES     LV_HOR_RES
#define SSD1963_VER_RES     LV_VER_RES
#define SSD1963_HDP         479
#define SSD1963_HT          531
#define SSD1963_HPS         43
#define SSD1963_LPS         8
#define SSD1963_HPW         10
#define SSD1963_VDP         271
#define SSD1963_VT          288
#define SSD1963_VPS         12
#define SSD1963_FPS         4
#define SSD1963_VPW         10
#define SSD1963_HS_NEG      0   /*Negative hsync*/
#define SSD1963_VS_NEG      0   /*Negative vsync*/
#define SSD1963_ORI         0   /*0, 90, 180, 270*/
#define SSD1963_COLOR_DEPTH 16
#endif

/*----------------
 *    ILI9341
 *--------------*/
#define USE_ILI9341        0
#if USE_ILI9341
#define ILI9341_DEBUG 0
#define ILI9341_ERR_CHECK 1
#define ILI9341_PAR_SUPPORT 1
#define ILI9341_SPI4WIRE_SUPPORT 1
#define ILI9341_SPI3WIRE_SUPPORT 1
#define ILI9341_EXTC_SUPPORT (0)
#define ILI9341_COLOR      (65) //Color Mode : 65K or 262K
#define ILI9341_MAX_SAMPLE (32) //Buffer spi is 64, so select 32 for 2byte per pixel
#define ILI9341_SERIAL_BYTESWAP   (0)  // { 0 : None, 1: CPU Swap, 2: ILI9341 swap }
#endif

/*----------------
 *    SSD1306
 *--------------*/
#define USE_SSD1306        1
#if USE_SSD1306
#define SSD1306_HOR_RES     LV_HOR_RES
#define SSD1306_VER_RES     LV_VER_RES
#define SSD1306_DEBUG    0
#define SSD1306_ERR_CHECK 0
#define SSD1306_I2C_SUPPORT  1
#define SSD1306_SPI_4_WIRE_SUPPORT 1
#define SSD1306_SPI_3_WIRE_SUPPORT 1
#endif

/*----------------
 *    R61581
 *--------------*/
#define USE_R61581          0
#if USE_R61581 != 0
#define R61581_HOR_RES      LV_HOR_RES
#define R61581_VER_RES      LV_VER_RES
#define R61581_HDP          479
#define R61581_HT           531
#define R61581_HPS          43
#define R61581_LPS          8
#define R61581_HPW          10
#define R61581_VDP          271
#define R61581_VT           319
#define R61581_VPS          12
#define R61581_FPS          4
#define R61581_VPW          10
#define R61581_HS_NEG       0       /*Negative hsync*/
#define R61581_VS_NEG       0       /*Negative vsync*/
#define R61581_ORI          180     /*0, 90, 180, 270*/
#define R61581_LV_COLOR_DEPTH 16
#endif

/*------------------------------
 *  ST7565 (Monochrome, low res.)
 *-----------------------------*/
#define USE_ST7565          0
#if USE_ST7565 != 0
/*No settings*/
#endif  /*USE_ST7565*/

/*-----------------------------------------
 *  Linux frame buffer device (/dev/fbx)
 *-----------------------------------------*/
#define USE_FBDEV           0
#if USE_FBDEV != 0
#define FBDEV_PATH          "/dev/fb0"
#endif

/*====================
 * Input devices
 *===================*/

/*--------------
 *    AR1000
 *--------------*/
#define USE_AR10XX (1)
#if (USE_AR10XX != 0)
#define AR10XX_SPI_SUPPORT (1)
#define AR10XX_I2C_SUPPORT (1)
#define AR10XX_UART_SUPPORT (0)
#define AR10XX_DEBUG (1)
#define AR10XX_ERR_CHECK (0)
#define AR10XX_VERIFY_ANSWER (1)
#define AR10XX_USE_IRQ (0)
#define AR10XX_COMPONENT (21) // Version of XX (10, 11, 20 , 21)
#endif

/*--------------
 *    XPT2046
 *--------------*/
#define USE_XPT2046         0
#if USE_XPT2046 != 0
#define XPT2046_HOR_RES     480
#define XPT2046_VER_RES     320
#define XPT2046_X_MIN       200
#define XPT2046_Y_MIN       200
#define XPT2046_X_MAX       3800
#define XPT2046_Y_MAX       3800
#define XPT2046_AVG         4
#define XPT2046_INV         0
#endif

/*-----------------
 *    FT5406EE8
 *-----------------*/
#define USE_FT5406EE8       0
#if USE_FT5406EE8
#define FT5406EE8_I2C_ADR   0x38                  /*7 bit address*/
#endif

/*-------------------------------
 *    Mouse or touchpad on PC
 *------------------------------*/
#define USE_MOUSE           0
#if USE_MOUSE
/*No settings*/
#endif


/*-------------------------------
 *   Keyboard of a PC
 *------------------------------*/
#define USE_KEYBOARD        0
#if USE_KEYBOARD
/*No settings*/
#endif

#endif  /*LV_DRV_CONF_H*/

#endif /*Remove this to enable the content*/
