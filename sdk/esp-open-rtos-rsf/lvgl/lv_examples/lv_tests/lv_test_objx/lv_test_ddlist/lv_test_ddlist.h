/**
 * @file lv_test_ddlist.h
 *
 */

#ifndef LV_TEST_DDLIST_H
#define LV_TEST_DDLIST_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#ifdef LV_CONF_INCLUDE_SIMPLE
#include "lvgl.h"
#include "lv_ex_conf.h"
#else
#include "../../../../lvgl/lvgl.h"
#include "../../../../lv_ex_conf.h"
#endif

#if USE_LV_DDLIST && USE_LV_TESTS

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Create drop down lists to test their functionalities
 */
void lv_test_ddlist_1(void);

/**********************
 *      MACROS
 **********************/

#endif /*USE_LV_DDLIST*/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*USE_LV_DDLIST && USE_LV_TESTS*/
