/**
 * @file lv_tutorial_styles.h
 *
 */

#ifndef LV_TUTORIAL_STYLES_H
#define LV_TUTORIAL_STYLES_H

#ifdef __cplusplus
lv_tutorialtern "C" {
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

#if USE_LV_TUTORIALS

    /*********************
     *      DEFINES
     *********************/

    /**********************
     *      TYPEDEFS
     **********************/

    /**********************
     * GLOBAL PROTOTYPES
     **********************/
    void lv_tutorial_styles(void);

    /**********************
     *      MACROS
     **********************/

#endif /*USE_LV_TUTORIALS*/

#ifdef __cplusplus
} /* lv_tutorialtern "C" */
#endif

#endif /*LV_TUTORIAL_STYLES_H*/
