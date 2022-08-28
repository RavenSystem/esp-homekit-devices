 /**
 * @file AR10XX.h
 * @author zaltora  (https://github.com/Zaltora)
 * @copyright MIT License.
 * @warn SPI:  Frequency 900KHz Max / CPOL: 0, CPHA: 1.
 * @warn SPI:  SIQ is THE IRQ signal
 * @warn I2C:  AR1020 don't support clock stretching.
 * @warn I2C:  Clock stretching can up to 50 us.
 * @warn I2C:  SDO is THE IRQ signal
 * @warn UART: Max baudrate is 9600
 */

//TODO: Buffered mod with a function ar10XX_update();
//TODO: Verify error_control only to test transmission error / not component error
//TODO: Add offset parameter for calibration position button (use range size of display to determine tactile size)
//TODO: Tactile component need trig lvgl API
//TODO: A NON-BLOCK API for tactile information.

#ifndef AR10XX_H
#define AR10XX_H
#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "../../lv_drv_conf.h"
#if USE_AR10XX != 0

#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include "../lv_drv_common.h"
#include "lvgl/lv_misc/lv_color.h"
#include "lvgl/lvgl.h"

/*********************
 *      DEFINES
 *********************/

//Update config localy, then update component.
#ifndef AR10XX_BUFFERED
#define AR10XX_BUFFERED (0)
#endif

#ifndef AR10XX_SPI_SUPPORT
#define AR10XX_SPI_SUPPORT (1)
#endif

#ifndef AR10XX_I2C_SUPPORT
#define AR10XX_I2C_SUPPORT (1)
#endif

#ifndef AR10XX_UART_SUPPORT
#define AR10XX_UART_SUPPORT (1)
#endif

#ifndef AR10XX_USE_IRQ
#define AR10XX_USE_IRQ (1)
#endif

//error list from AR10XX
#define AR10XX_ERR_SUCCESS               (0x00)
#define AR10XX_ERR_CMD_UNRECOGNIZED      (0x01)
#define AR10XX_ERR_HEADER_UNRECOGNIZED   (0x03)
#define AR10XX_ERR_CMD_TIMEOUT           (0x04)
#define AR10XX_ERR_CANCEL_CALIB_MODE     (0xFC)

#define AR10XX_I2C_ADDR             (0x4D)
#define AR10XX_READ_TIMEOUT         (200) //in ms
#define AR10XX_READ_DELAY_LOOP      (50)  //in ms
#define AR10XX_EEPROM_USER_SIZE     (128)

//default value for resistive touch
#define AR10XX_DEFAULT_X1     (260)
#define AR10XX_DEFAULT_Y1     (3820)
#define AR10XX_DEFAULT_X2     (3840)
#define AR10XX_DEFAULT_Y2     (330)
#define AR10XX_READ_INVERT_XY (false)

/**********************
 *      TYPEDEFS
 **********************/

/**
 * I/O protocols
 */
typedef enum
{
    AR10XX_PROTO_I2C = 0, //!<  I2C 2 Wire
    AR10XX_PROTO_SPI, //!<  SPI 4 Wire
    AR10XX_PROTO_UART, //<! UART 2 Wire
} ar10xx_protocol_t;

typedef enum
{
    AR10XX_SAMPLING_1 = 1,
    AR10XX_SAMPLING_4 = 4,
    AR10XX_SAMPLING_8 = 8,
    AR10XX_SAMPLING_16 = 16,
    AR10XX_SAMPLING_32 = 32,
    AR10XX_SAMPLING_64 = 64,
    AR10XX_SAMPLING_128 = 128,
} ar10xx_sampling_t;

/**
 * Device descriptor
 */
typedef struct
{
    ar10xx_protocol_t protocol;    //!< Protocol used
    union
    {
#if (AR10XX_I2C_SUPPORT)
        lv_i2c_handle_t i2c_dev;   //!< I2C device descriptor
#endif
#if (AR10XX_SPI_SUPPORT)
        lv_spi_handle_t spi_dev;   //!< SPI device descriptor
#endif
#if (AR10XX_UART_SUPPORT)
        lv_uart_handle_t uart_dev;   //!< uart device descriptor
#endif
    };
#if AR10XX_USE_IRQ
    volatile uint8_t count_irq;     //!< Store Interupt information
#endif
    lv_screen_size_t size;
    lv_indev_limit_t cal;
    int16_t x;
    int16_t y;
    lv_indev_state_t p;
    lv_rotation_t ro;
    lv_rotation_t rc;
    uint8_t opt_enable : 1;
} ar10xx_t;

typedef struct {
    uint16_t type ;
    uint8_t controler_type : 6 ;
    uint8_t resolution : 2 ;
} ar10xx_id_t;

typedef union
{
    struct {
        uint8_t spe_use1;
        uint8_t spe_use2;
        uint8_t touch_treshold;
        uint8_t sensitivity_filter;
        uint8_t sampling_fast;
        uint8_t sampling_slow;
        uint8_t accurancy_filter_fast;
        uint8_t accurancy_filter_slow;
        uint8_t speed_treshold;
        uint8_t spe_use3;
        uint8_t sleep_delay;
        uint8_t pen_up_delay;
        uint8_t touch_mode;
        uint8_t touch_options;
        uint8_t calibration_inset;
        uint8_t pen_state_report_delay;
        uint8_t spe_use4;
        uint8_t touch_report_delay;
        uint8_t spe_use5;
    };
    uint8_t reg_data[19];
} ar10xx_regmap_t ;

/*
 * Touchmode register (Datasheet: page 27)
 */
typedef union
{
    struct {
        uint8_t pu : 3; //Pen up state bits
        uint8_t pm : 2; //Pen mouvement state bits
        uint8_t pd : 3; //Pen Down state bits
    };
    uint8_t value;
} ar10xx_touchmode_t ;

/*
 * Touchoption register (Datasheet: page 28)
 */
typedef union
{
    struct {
        uint8_t cce : 1; //Calibrate coordinate bit
        uint8_t W48 : 1; //4/8 wire selection bit
        uint8_t : 6;
    };
    uint8_t value;
} ar10xx_touchoption_t ;

/**********************
 *      MACROS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

//Touch need be disable when you you use this API
// 0 - 255
int ar10xx_set_touch_treshold( ar10xx_t *dev, uint8_t value);
//0 - 10
int ar10xx_set_sensitivity_filter( ar10xx_t *dev, uint8_t value);
//1,4,8,16,32,64,128
int ar10xx_set_sampling_fast( ar10xx_t *dev, ar10xx_sampling_t value);
//1,4,8,16,32,64,128
int ar10xx_set_sampling_slow( ar10xx_t *dev, ar10xx_sampling_t value);
//1 - 8
int ar10xx_set_accuracy_filter_fast( ar10xx_t *dev, uint8_t value);
//1 - 8
int ar10xx_set_accuracy_filter_slow( ar10xx_t *dev, uint8_t value);
// 0 - 255
int ar10xx_set_speed_treshold( ar10xx_t *dev, uint8_t value);
// 0 - 255  (* 100 ms )
int ar10xx_set_sleep_delay( ar10xx_t *dev, uint8_t value);
// 0 - 255  (* 240 us )
int ar10xx_set_penup_delay( ar10xx_t *dev, uint8_t value);
//complex register
int ar10xx_set_touch_mode( ar10xx_t *dev, ar10xx_touchmode_t reg);
//complex register
int ar10xx_set_touch_options( ar10xx_t *dev, ar10xx_touchoption_t reg);
//0 - 40
int ar10xx_set_calibration_inset( ar10xx_t *dev, uint8_t value);
//0 - 255
int ar10xx_set_pen_state_report_delay( ar10xx_t *dev, uint8_t value);
//0 - 255
int ar10xx_set_touch_report_delay( ar10xx_t *dev, uint8_t value);

//

int ar10xx_get_version( ar10xx_t *dev, ar10xx_id_t* version);
int ar10xx_disable_touch( ar10xx_t *dev);
int ar10xx_enable_touch( ar10xx_t *dev);


int ar10xx_init(ar10xx_t *dev, uint16_t width, uint16_t heigth, lv_rotation_t rotation);

//Set default value for touch screen (need reboot after this)
int ar10xx_factory_setting(ar10xx_t *dev);

//save registers to EEPROM
int ar10xx_save_configs(ar10xx_t *dev);

//load EEPROM to registers
int ar10xx_load_configs(ar10xx_t *dev);

//read configs from registers
int ar10xx_read_configs(ar10xx_t *dev, ar10xx_regmap_t* regmap);

//write configs to registers
int ar10xx_write_configs(ar10xx_t *dev, const ar10xx_regmap_t* regmap);

//Read user data (128 byte max)
int ar10xx_eeprom_read(ar10xx_t *dev, uint8_t addr, uint8_t* buf, uint8_t size);

//write user data  (128 byte max)
int ar10xx_eeprom_write(ar10xx_t *dev, uint8_t addr, const uint8_t* data, uint8_t size);


int ar10xx_set_calib_data(ar10xx_t *dev, lv_point_t* pts, int16_t offset);
void ar10xx_set_rotation(ar10xx_t *dev, lv_rotation_t degree);

//lvlg input read
bool ar10xx_input_get_raw(lv_indev_data_t * data);
bool ar10xx_input_get_calib(lv_indev_data_t * data);

//function to call in interupt routine
#if AR10XX_USE_IRQ
inline void INTERUPT_ATTRIBUTE ar10xx_irq(ar10xx_t* dev)
{
    if(dev->count_irq != 0xFF) dev->count_irq++ ; //event on irq
}
#endif


#ifdef __cplusplus
}
#endif
#endif /* USE_AR10XX*/
#endif //AR10XX_H
