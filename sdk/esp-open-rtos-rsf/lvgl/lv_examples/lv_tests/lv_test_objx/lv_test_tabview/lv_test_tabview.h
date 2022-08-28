/**
 * @file lv_test_tabview.h
 *
 */

#ifndef LV_TEST_TABVIEW_H
#define LV_TEST_TABVIEW_H

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

#if USE_LV_TABVIEW && USE_LV_TESTS

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
 * Create tab views to test their functionalities
 */
void lv_test_tabview_1(void);

/**********************
 *      MACROS
 **********************/

#endif /*USE_LV_TABVIEW && USE_LV_TESTS*/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*LV_TEST_TABVIEW_H*/
