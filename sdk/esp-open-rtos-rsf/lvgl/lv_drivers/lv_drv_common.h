/**
 * @file lv_drv_common.h
 * 
 */ 

#ifndef LV_DRV_COMMON_H
#define LV_DRV_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "../lv_drv_conf.h"
#include "lvgl/lvgl.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *     TYPEDEFS
 **********************/
typedef enum
{
    LV_ROT_DEGREE_0 = 0,
    LV_ROT_DEGREE_90 = 1,
    LV_ROT_DEGREE_180 = 2,
    LV_ROT_DEGREE_270 = 3,
} lv_rotation_t;

typedef struct
{
    uint16_t width;
    uint16_t heigth;
} lv_screen_size_t;

typedef struct
{
    int16_t xmin;
    int16_t xmax;
    int16_t ymin;
    int16_t ymax;
} lv_indev_limit_t;

/**********************
 *  GLOBAL MACROS
 **********************/
#define GET_POINT_CALIB(p, size, min, max)   (((int32_t)p - (int32_t)min) * (int32_t)size) / ((int32_t)max - (int32_t)min);
#define SWAP(x, y)    do { typeof(x) SWAP = x; x = y; y = SWAP; } while (0)

/**********************
 *  GLOBAL PROTOTYPES
 **********************/
static inline int i2c_send(const lv_i2c_handle_t i2c_dev, uint8_t reg, uint8_t* data, uint8_t len)
{
    return lv_i2c_write(i2c_dev, &reg, data, len);
}

#if LV_DRIVER_ENABLE_PAR
static inline int par_send(const lv_spi_handle_t spi_dev, bool dc, uint8_t* data, uint8_t len, uint8_t wordsize)
{
    lv_par_wr_cs(spi_dev, false);
    lv_par_wr_dc(spi_dev, dc); /* command mode */
    int err = lv_par_write(spi_dev, data, len, wordsize);
    lv_par_wr_cs(spi_dev, true);
    return err;
}
#endif

#if LV_DRIVER_ENABLE_SPI
static inline int spi3wire_send(const lv_spi_handle_t spi_dev, bool dc, uint8_t* data, uint8_t len, uint8_t wordsize)
{
    lv_spi_wr_cs(spi_dev, false);
    lv_spi_set_preemble(spi_dev, LV_SPI_COMMAND, dc, 1);
    int err = lv_spi_transaction(spi_dev, NULL, data, len, wordsize);
    lv_spi_clr_preemble(spi_dev, LV_SPI_COMMAND);
    lv_spi_wr_cs(spi_dev, true);
    return err;
}

static inline int spi4wire_send(const lv_spi_handle_t spi_dev, bool dc, uint8_t* data, uint8_t len, uint8_t wordsize)
{
    lv_spi_wr_cs(spi_dev, false);
    lv_spi_wr_dc(spi_dev, dc); /* command mode */
    int err = lv_spi_transaction(spi_dev, NULL, data, len, wordsize);
    lv_spi_wr_cs(spi_dev, true);
    return err;
}
#endif

static inline int common_indev_calibration(lv_indev_limit_t *cal, lv_point_t* pts, uint16_t width, uint16_t heigth, int16_t offset)
{
    if(!pts || !cal)
    {
        return -1;
    }
    //If offset is too high, the calibration can't be good. Just add a small limitation
    else if ((offset >= width/2) || (offset >= heigth/2))
    {
        return -1;
    }

    //x process, found z = ax+b with point 1 and 3.
    //b1 is the point at 0, found with the point 3 coordinate.
    int32_t  b1 = (int32_t)( (pts[2].x*(width-2*offset)) - ((pts[2].x-pts[0].x)*(width-offset)) ) / (int32_t)(width-2*offset);

    //found y with the width size
    int32_t  z1 = (int32_t)( ((pts[2].x-pts[0].x)*width) + b1 * (width-2*offset) ) / (int32_t)(width-2*offset);

    //x process, found y = ax+b with point 2 and 4.
    //b1 is the point at 0, found with the point 3 coordinate.
    int32_t  b2 = (int32_t)( (pts[1].x*(width-2*offset)) - ((pts[1].x-pts[3].x)*(width-offset)) ) / (int32_t)(width-2*offset);

    //found y with the width size
    int32_t  z2 = (int32_t)( ((pts[1].x-pts[3].x)*width) + b2 * (width-2*offset) ) / (int32_t)(width-2*offset);

    cal->xmin = (b1+b2)/2;
    cal->xmax = (z1+z2)/2;

    //y process, found z = ax+b with point 1 and 3.
    //b1 is the point at 0, found with the point 3 coordinate.
    b1 = ( (pts[2].y*(heigth-2*offset)) - ((pts[2].y-pts[0].y)*(heigth-offset)) ) / (heigth-2*offset);

    //found y with the width size
    z1 = ( ((pts[2].y-pts[0].y)*heigth) + b1 * (heigth-2*offset) ) / (heigth-2*offset);

    //y process, found y = ax+b with point 2 and 4.
    //b1 is the point at 0, found with the point 3 coordinate.
    b2 = ( (pts[3].y*(heigth-2*offset)) - ((pts[3].y-pts[1].y)*(heigth-offset)) ) / (heigth-2*offset);

    //found y with the width size
    z2 = ( ((pts[3].y-pts[1].y)*heigth) + b2 * (heigth-2*offset) ) / (heigth-2*offset);

    cal->ymin = (b1+b2)/2;
    cal->ymax = (z1+z2)/2;
    return 0;
}

static inline void common_indev_rotation(lv_indev_limit_t *cal, lv_screen_size_t *size, lv_rotation_t degree)
{
    //+/-180 degree rotation
    if ((degree == 2) || (degree == -2))
    {
        SWAP(cal->xmin, cal->xmax);
        SWAP(cal->ymin, cal->ymax);
    }
    //+90 or -270 degree rotation (right to left)
    else if ((degree == 1) || (degree == -3))
    {
        int16_t temp = cal->xmin;
        cal->xmin = cal->ymax;
        cal->ymax = cal->xmax;
        cal->xmax = cal->ymin;
        cal->ymin = temp;
        SWAP(size->width, size->heigth);
    }
    //-90 or +270 degree rotation (left to right)
    else if ((degree == -1) || (degree == 3))
    {
        int16_t temp = cal->xmin;
        cal->xmin = cal->ymin;
        cal->ymin = cal->xmax;
        cal->xmax = cal->ymax;
        cal->ymax = temp;
        SWAP(size->width, size->heigth);
    }
}


#ifdef __cplusplus
}
#endif
#endif /*LV_DRV_COMMON_H */
