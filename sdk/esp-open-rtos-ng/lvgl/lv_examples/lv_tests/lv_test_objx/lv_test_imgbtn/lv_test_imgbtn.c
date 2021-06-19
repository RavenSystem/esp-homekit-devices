/**
 * @file lv_test_imgbtn.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_test_imgbtn.h"
#if USE_LV_IMGBTN && USE_LV_TESTS

#if LV_EX_PRINTF
#include <stdio.h>
#endif

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
lv_res_t imgbtn_clicked(lv_obj_t * imgbtn);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * Create imgbtns to test their functionalities
 */
void lv_test_imgbtn_1(void)
{
    /* Create an image button and set images for it*/
    lv_obj_t * imgbtn1 = lv_imgbtn_create(lv_scr_act(), NULL);
    lv_obj_set_pos(imgbtn1, 10, 10);
    lv_imgbtn_set_toggle(imgbtn1, true);

    LV_IMG_DECLARE(imgbtn_img_1);
    LV_IMG_DECLARE(imgbtn_img_2);
    LV_IMG_DECLARE(imgbtn_img_3);
    LV_IMG_DECLARE(imgbtn_img_4);

    lv_imgbtn_set_src(imgbtn1, LV_BTN_STATE_REL, &imgbtn_img_1);
    lv_imgbtn_set_src(imgbtn1, LV_BTN_STATE_PR, &imgbtn_img_2);
    lv_imgbtn_set_src(imgbtn1, LV_BTN_STATE_TGL_REL, &imgbtn_img_3);
    lv_imgbtn_set_src(imgbtn1, LV_BTN_STATE_TGL_PR, &imgbtn_img_4);

    lv_imgbtn_set_action(imgbtn1, LV_BTN_ACTION_CLICK, imgbtn_clicked);

    /*Add a label*/
    lv_obj_t * label = lv_label_create(imgbtn1, NULL);
    lv_label_set_text(label, "Button 1");

    /*Copy the image button*/
    lv_obj_t * imgbtn2 = lv_imgbtn_create(lv_scr_act(), imgbtn1);
    lv_imgbtn_set_state(imgbtn2, LV_BTN_STATE_TGL_REL);
    lv_obj_align(imgbtn2, imgbtn1, LV_ALIGN_OUT_RIGHT_MID, 20, 0);

    label = lv_label_create(imgbtn2, NULL);
    lv_label_set_text(label, "Button 2");
}


/**********************
 *   STATIC FUNCTIONS
 **********************/

lv_res_t imgbtn_clicked(lv_obj_t * imgbtn)
{
    (void) imgbtn; /*Unused*/

#if LV_EX_PRINTF
    printf("Clicked\n");
#endif

    return LV_RES_OK;
}

#endif /*USE_LV_IMGBTN && USE_LV_TESTS*/
