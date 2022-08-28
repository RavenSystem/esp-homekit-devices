/**
 * @file lv_tutorial_keyboard_simkpad.c
 *
 */

/*
 * -------------------------------------------
 * Learn how to use a keyboard/keypad device
 * -------------------------------------------
 *
 * You need two things to use keypad/keyboard:
 *
 * INPUT DEVICE DRIVER
 * - Similarly to touchpad you need to register an 'lv_indev_drv_t' driver
 * - For control keys you should use LV_GROUP_KEY_... from lv_group.h (e.g. LV_GROUP_KEY_NEXT)
 * -
 *
 *
 * OBJECT GROUP
 * - You can iterate through objects in a group (like using 'tab' on PC) and adjust/modify them
 * - Firstly you need to create an object group: `lv_group_t *g = lv_group_create();`
 * - And add objects to it: `lv_group_add_obj(g, btn1);`
 * - Then you can send data to the object in focus: lv_group_send_data(g, 'a');
 *                                                  lv_group_send_data(g, LV_GROUP_UP);
 * - Or focus on the next/prev. object:  lv_group_focus_next(g);
 *
 */
/*********************
 *      INCLUDES
 *********************/
#include "lv_tutorial_keyboard.h"
#if USE_LV_TUTORIALS && USE_LV_GROUP

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void gui_create(void);
static void kaypad_create(void);
static bool emulated_keypad_read(lv_indev_data_t * data);
static lv_res_t mbox_action(lv_obj_t * btn, const char * txt);
static lv_res_t enable_action(lv_obj_t * btn);
static lv_res_t keypad_btn_press(lv_obj_t * btn);
static lv_res_t keypad_btn_release(lv_obj_t * btn);

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_obj_t * btn_enable;           /*An enable button*/
static lv_style_t style_mbox_bg;        /*Black bg. style with opacity*/
static lv_group_t * g;                  /*An Object Group*/
static lv_indev_t * emulated_kp_indev;  /*The input device of the emulated keypad*/
static lv_indev_state_t last_state = LV_INDEV_STATE_REL;
static uint32_t last_key = 0;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * Create a simple GUI to demonstrate encoder control capability
 * kp_indev optinonally pass a keypad input device to control the object (NULL if unused)
 */
void lv_tutorial_keyboard(lv_indev_t * kp_indev)
{
    /*Register the emulated keyboard*/
    lv_indev_drv_t  kp_drv;
    lv_indev_drv_init(&kp_drv);
    kp_drv.type = LV_INDEV_TYPE_KEYPAD;
    kp_drv.read = emulated_keypad_read;
    emulated_kp_indev = lv_indev_drv_register(&kp_drv);

    /*Create an object group*/
    g = lv_group_create();

    /*Assig the input device(s) to the created group*/
    lv_indev_set_group(emulated_kp_indev, g);
    if(kp_indev) lv_indev_set_group(kp_indev, g);

    /*Create a demo GUI*/
    gui_create();

    /*Create virtual encoder*/
    kaypad_create();
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Create a demo GUI
 */
static void gui_create(void)
{
    /*Create a drop down list*/
    lv_obj_t * ddlist = lv_ddlist_create(lv_scr_act(), NULL);
    lv_ddlist_set_options(ddlist, "Low\nMedium\nHigh");
    lv_obj_set_pos(ddlist, LV_DPI / 4, LV_DPI / 4);
    lv_group_add_obj(g, ddlist);                    /*Add the object to the group*/

    /*Create a holder and check boxes on it*/
    lv_obj_t * holder = lv_cont_create(lv_scr_act(), NULL);   /*Create a transparent holder*/
    lv_cont_set_fit(holder, true, true);
    lv_cont_set_layout(holder, LV_LAYOUT_COL_L);
    lv_obj_set_style(holder, &lv_style_transp);
    lv_obj_align(holder, ddlist, LV_ALIGN_OUT_RIGHT_TOP, LV_DPI / 4, 0);

    lv_obj_t * cb = lv_cb_create(holder, NULL);     /*First check box*/
    lv_cb_set_text(cb, "Red");
    lv_group_add_obj(g, cb);                        /*Add to the group*/

    cb = lv_cb_create(holder, cb);                  /*Copy the first check box. Automatically added to the same group*/
    lv_cb_set_text(cb, "Green");

    cb = lv_cb_create(holder, cb);                  /*Copy the second check box. Automatically added to the same group*/
    lv_cb_set_text(cb, "Blue");

    /*Create a sliders*/
    lv_obj_t * slider = lv_slider_create(lv_scr_act(), NULL);
    lv_obj_set_size(slider, LV_DPI, LV_DPI / 3);
    lv_obj_align(slider, holder, LV_ALIGN_OUT_RIGHT_TOP, LV_DPI / 4, 0);
    lv_bar_set_range(slider, 0, 20);
    lv_group_add_obj(g, slider);                    /*Add to the group*/

    /*Create a button*/
    btn_enable = lv_btn_create(lv_scr_act(), NULL);
    lv_btn_set_action(btn_enable, LV_BTN_ACTION_CLICK, enable_action);
    lv_cont_set_fit(btn_enable, true, true);
    lv_group_add_obj(g, btn_enable);                /*Add to the group*/
    lv_obj_t * l = lv_label_create(btn_enable, NULL);
    lv_label_set_text(l, "Message");
    lv_obj_align(btn_enable, slider, LV_ALIGN_OUT_BOTTOM_MID, 0, LV_DPI / 2);

    /* Create a dark plain style for a message box's background (modal)*/
    lv_style_copy(&style_mbox_bg, &lv_style_plain);
    style_mbox_bg.body.main_color = LV_COLOR_BLACK;
    style_mbox_bg.body.grad_color = LV_COLOR_BLACK;
    style_mbox_bg.body.opa = LV_OPA_50;
}

/**
 * Create virtual keypad using 4 buttons:
 * - Next: focus on the next object in the group
 * - Increment: increment the object value
 * - Decrement: decrement the object value
 * - Enter: Select something
 */
static void kaypad_create(void)
{
    /*Next button*/
    lv_obj_t * btn_next = lv_btn_create(lv_scr_act(), NULL);
    lv_btn_set_action(btn_next, LV_BTN_ACTION_PR, keypad_btn_press);
    lv_btn_set_action(btn_next, LV_BTN_ACTION_CLICK, keypad_btn_release);
    lv_btn_set_action(btn_next, LV_BTN_ACTION_LONG_PR, keypad_btn_release);
    lv_btn_set_fit(btn_next, true, true);
    lv_obj_set_free_num(btn_next, LV_GROUP_KEY_NEXT);
    lv_obj_t * l = lv_label_create(btn_next, NULL);
    lv_label_set_text(l, "Next");
    lv_obj_align(btn_next, NULL, LV_ALIGN_IN_BOTTOM_LEFT, LV_DPI / 4, - LV_DPI / 4);

    /*Increment button*/
    lv_obj_t * btn_inc = lv_btn_create(lv_scr_act(), btn_next);
    lv_obj_set_free_num(btn_inc, LV_GROUP_KEY_LEFT);
    l = lv_label_create(btn_inc, NULL);
    lv_label_set_text(l, "Dec");
    lv_obj_align(btn_inc, btn_next, LV_ALIGN_OUT_RIGHT_MID, LV_DPI / 4, 0);

    /*Decrement button*/
    lv_obj_t * btn_dec = lv_btn_create(lv_scr_act(), btn_next);
    lv_obj_set_free_num(btn_dec, LV_GROUP_KEY_RIGHT);
    l = lv_label_create(btn_dec, NULL);
    lv_label_set_text(l, "Inc");
    lv_obj_align(btn_dec, btn_inc, LV_ALIGN_OUT_RIGHT_MID, LV_DPI / 4, 0);

    /*Enter button*/
    lv_obj_t * btn_enter = lv_btn_create(lv_scr_act(), btn_next);
    lv_obj_set_free_num(btn_enter, LV_GROUP_KEY_ENTER);
    l = lv_label_create(btn_enter, NULL);
    lv_label_set_text(l, "Enter");
    lv_obj_align(btn_enter, btn_dec, LV_ALIGN_OUT_RIGHT_MID, LV_DPI / 4, 0);
}

static bool emulated_keypad_read(lv_indev_data_t * data)
{
    data->key = last_key;
    data->state = last_state;
    return false;
}

/**
 * Called when the Enable button is released. Show a message box to really enable or not?
 * @param btn pointer to the Enable button
 * @param indev_proc pointer to the caller display input or NULL if the encoder used
 * @return LV_RES_OK: because the button is not deleted
 */
static lv_res_t enable_action(lv_obj_t * btn)
{
    /*If the butto nsi released the show message box to be sure about the Enable*/
    if(lv_btn_get_state(btn) == LV_BTN_STATE_REL) {
        /* Create a dark screen sized bg. with opacity to show
         * the other objects are not available now*/
        lv_obj_t * bg = lv_obj_create(lv_scr_act(), NULL);
        lv_obj_set_protect(bg, LV_PROTECT_PARENT);                  /*The page screen moves it to scrollable area*/
        lv_obj_set_parent(bg, lv_scr_act());                         /*So move it back when protected*/
        lv_obj_set_style(bg, &style_mbox_bg);
        lv_obj_set_size(bg, LV_HOR_RES, LV_VER_RES);
        lv_obj_set_pos(bg, 0, 0);
        lv_obj_set_click(bg, false);                        /*For test disable click to enable the buttons under it (else this object would be on the top)*/

        /*Create a message box*/
        lv_obj_t * mbox = lv_mbox_create(bg, NULL);
        lv_mbox_set_text(mbox, "Turn on something?");
        lv_group_add_obj(g, mbox);          /*Add to he group*/

        /*Add two buttons*/
        static const char * btns[] = {"Yes", "No", ""};
        lv_mbox_add_btns(mbox, btns, mbox_action);

        lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, - LV_DPI / 2);

        /*Focus on the new message box, can freeze focus on it*/
        lv_group_focus_obj(mbox);
        lv_group_focus_freeze(g, true);
    }
    /*Just disable without message*/
    else {
        lv_btn_set_state(btn_enable, LV_BTN_STATE_REL);
    }
    return LV_RES_OK;
}

/**
 * Called when a message box button is clicked
 * @param btn pointer to the 'Yes' button
 * @return LV_ACTION_RES_INV: because the button along with the message box will be deleted
 */
static lv_res_t mbox_action(lv_obj_t * btn, const char * txt)
{
    lv_group_focus_freeze(g, false);        /*Release the freeze*/
    lv_obj_t * mbox = lv_mbox_get_from_btn(btn);
    lv_obj_del(lv_obj_get_parent(mbox));    /*Delete the black background. (it will delete the mbox too)*/


    /*Mark the enabled state by toggling the button*/
    if(strcmp(txt, "No") == 0)  lv_btn_set_state(btn_enable, LV_BTN_STATE_REL);
    else if(strcmp(txt, "Yes") == 0)  lv_btn_set_state(btn_enable, LV_BTN_STATE_TGL_REL);

    return LV_RES_INV;
}

/**
 * Called when the Keypad button is pressed
 * @param btn pointer to the button
 * @param indev_proc pointer to the caller display input
 * @return LV_RES_OK: because the button is not deleted
 */
static lv_res_t keypad_btn_press(lv_obj_t * btn)
{
    last_key = lv_obj_get_free_num(btn);    /*Save the key*/
    last_state = LV_INDEV_STATE_PR;         /*Save the state*/

    return LV_RES_OK;
}

/**
 * Called when the Next button is released
 * @param btn pointer to the button
 * @param indev_proc pointer to the caller display input
 * @return LV_RES_OK: because the button is not deleted
 */
static lv_res_t keypad_btn_release(lv_obj_t * btn)
{
    (void)btn;      /*Unused*/

    last_state = LV_INDEV_STATE_REL;        /*Save the new state but leave the last key*/

    return LV_RES_OK;
}


#endif /*USE_LV_TUTORIALS*/
