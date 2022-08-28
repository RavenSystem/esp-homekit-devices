/**
 * @file lv_tutorial_keyboard.h
 *
 */

#ifndef LV_TUTORIAL_KEYBOARD_H
#define LV_TUTORIAL_KEYBOARD_H

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
#include "../../../lvgl/lvgl.h"
#include "../../../lv_ex_conf.h"
#endif

#if USE_LV_TUTORIALS && USE_LV_GROUP

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
 * Create a simple GUI to demonstrate encoder control capability
 * kp_indev optinonally pass a keypad input device to control the object (NULL if unused)
 */
void lv_tutorial_keyboard(lv_indev_t * kp_indev);

/**********************
 *      MACROS
 **********************/

#endif /*USE_LV_TUTORIALS*/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*LV_TUTORIAL_KEYBOARD_H*/
