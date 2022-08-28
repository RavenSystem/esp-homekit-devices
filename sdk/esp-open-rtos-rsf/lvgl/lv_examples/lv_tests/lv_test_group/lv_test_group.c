/**
 * @file lv_test_group.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "stdio.h"
#include "lv_test_group.h"
#if USE_LV_GROUP && USE_LV_TESTS

#include "lvgl/lv_hal/lv_hal_indev.h"

#if LV_EX_KEYBOARD || LV_EX_ENCODER
#include "lv_drv_conf.h"
#endif

#if LV_EX_KEYBOARD
#include "lv_drivers/indev/keyboard.h"
#endif

#if LV_EX_ENCODER
#include "lv_drivers/indev/encoder.h"
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
/*To emulate some keys on the window header*/
static bool win_btn_read(lv_indev_data_t * data);
static lv_res_t win_btn_press(lv_obj_t * btn);
static lv_res_t win_btn_click(lv_obj_t * btn);

static void group_focus_cb(lv_group_t * group);

/*Dummy action functions*/
static lv_res_t press_action(lv_obj_t * btn);
static lv_res_t select_action(lv_obj_t * btn);
static lv_res_t change_action(lv_obj_t * btn);
static lv_res_t click_action(lv_obj_t * btn);
static lv_res_t long_press_action(lv_obj_t * btn);

/**********************
 *  STATIC VARIABLES
 **********************/
static uint32_t last_key;
static lv_indev_state_t last_key_state = LV_INDEV_STATE_REL;
static lv_group_t * g;
static lv_obj_t * win;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/


bool ta_chk(lv_obj_t * ta, uint32_t c)
{
    static bool run;

    /*Prevent to be called from every `lv_ta_add_char`*/
    if(run) return true;

    run = true;

    lv_ta_add_char(ta, c);
    lv_ta_add_char(ta, '.');

    run = false;
    return false;
}

/**
 * Create base groups to test their functionalities
 */
void lv_test_group_1(void)
{

    g = lv_group_create();
    lv_group_set_focus_cb(g, group_focus_cb);

    /*A keyboard will be simulated*/
    lv_indev_drv_t sim_kb_drv;
    sim_kb_drv.type = LV_INDEV_TYPE_KEYPAD;
    sim_kb_drv.read = win_btn_read;
    lv_indev_t * win_kb_indev = lv_indev_drv_register(&sim_kb_drv);
    lv_indev_set_group(win_kb_indev, g);

#if LV_EX_KEYBOARD
    lv_indev_drv_t rael_kb_drv;
    rael_kb_drv.type = LV_INDEV_TYPE_KEYPAD;
    rael_kb_drv.read = keyboard_read;
    lv_indev_t * real_kb_indev = lv_indev_drv_register(&rael_kb_drv);
    lv_indev_set_group(real_kb_indev, g);
#endif

#if LV_EX_ENCODER
    lv_indev_drv_t enc_drv;
    enc_drv.type = LV_INDEV_TYPE_ENCODER;
    enc_drv.read = encoder_read;
    lv_indev_t * enc_indev = lv_indev_drv_register(&enc_drv);
    lv_indev_set_group(enc_indev, g);
#endif

    /*Create a window to hold all the objects*/
    static lv_style_t win_style;
    lv_style_copy(&win_style, &lv_style_transp);
    win_style.body.padding.hor = LV_DPI / 4;
    win_style.body.padding.ver = LV_DPI / 4;
    win_style.body.padding.inner = LV_DPI / 4;

    win = lv_win_create(lv_scr_act(), NULL);
    lv_win_set_title(win, "Group test");
    lv_page_set_scrl_layout(lv_win_get_content(win), LV_LAYOUT_PRETTY);
    lv_win_set_style(win, LV_WIN_STYLE_CONTENT_SCRL, &win_style);
    lv_group_add_obj(g, lv_win_get_content(win));

    lv_obj_t * win_btn = lv_win_add_btn(win, SYMBOL_RIGHT, win_btn_click);
    lv_btn_set_action(win_btn, LV_BTN_ACTION_PR, win_btn_press);
    lv_obj_set_free_num(win_btn, LV_GROUP_KEY_RIGHT);
    lv_obj_set_protect(win_btn, LV_PROTECT_CLICK_FOCUS);

    win_btn = lv_win_add_btn(win, SYMBOL_NEXT, win_btn_click);
    lv_btn_set_action(win_btn, LV_BTN_ACTION_PR, win_btn_press);
    lv_obj_set_free_num(win_btn, LV_GROUP_KEY_NEXT);
    lv_obj_set_protect(win_btn, LV_PROTECT_CLICK_FOCUS);

    win_btn = lv_win_add_btn(win, SYMBOL_OK, win_btn_click);
    lv_btn_set_action(win_btn, LV_BTN_ACTION_PR, win_btn_press);
    lv_obj_set_free_num(win_btn, LV_GROUP_KEY_ENTER);
    lv_obj_set_protect(win_btn, LV_PROTECT_CLICK_FOCUS);

    win_btn = lv_win_add_btn(win, SYMBOL_PREV, win_btn_click);
    lv_btn_set_action(win_btn, LV_BTN_ACTION_PR, win_btn_press);
    lv_obj_set_free_num(win_btn, LV_GROUP_KEY_PREV);
    lv_obj_set_protect(win_btn, LV_PROTECT_CLICK_FOCUS);

    win_btn = lv_win_add_btn(win, SYMBOL_LEFT, win_btn_click);
    lv_btn_set_action(win_btn, LV_BTN_ACTION_PR, win_btn_press);
    lv_obj_set_free_num(win_btn, LV_GROUP_KEY_LEFT);
    lv_obj_set_protect(win_btn, LV_PROTECT_CLICK_FOCUS);

    win_btn = lv_win_add_btn(win, SYMBOL_DUMMY"a", win_btn_click);
    lv_btn_set_action(win_btn, LV_BTN_ACTION_PR, win_btn_press);
    lv_obj_set_free_num(win_btn, 'a');
    lv_obj_set_protect(win_btn, LV_PROTECT_CLICK_FOCUS);

    lv_obj_t * obj;

    obj = lv_obj_create(win, NULL);
    lv_obj_set_style(obj, &lv_style_plain_color);
    lv_group_add_obj(g, obj);

    obj = lv_label_create(win, NULL);
    lv_group_add_obj(g, obj);

    obj = lv_btn_create(win, NULL);
    lv_group_add_obj(g, obj);
    lv_btn_set_toggle(obj, true);
    lv_btn_set_action(obj, LV_BTN_ACTION_CLICK, click_action);
    lv_btn_set_action(obj, LV_BTN_ACTION_PR, press_action);
    lv_btn_set_action(obj, LV_BTN_ACTION_LONG_PR, long_press_action);
    obj = lv_label_create(obj, NULL);
    lv_label_set_text(obj, "Button");

    obj = lv_cb_create(win, NULL);
    lv_cb_set_action(obj, select_action);
    lv_group_add_obj(g, obj);

    obj = lv_bar_create(win, NULL);
    lv_bar_set_value(obj, 60);
    lv_group_add_obj(g, obj);

    obj = lv_slider_create(win, NULL);
    lv_slider_set_range(obj, 0, 10);
    lv_slider_set_action(obj, change_action);
    lv_group_add_obj(g, obj);

    obj = lv_sw_create(win, NULL);
    lv_sw_set_action(obj, change_action);
    lv_group_add_obj(g, obj);

    obj = lv_ddlist_create(win, NULL);
    lv_ddlist_set_options(obj, "Item1\nItem2\nItem3\nItem4\nItem5\nItem6");
    lv_ddlist_set_fix_height(obj, LV_DPI);
    lv_ddlist_set_action(obj, select_action);
    lv_group_add_obj(g, obj);

    obj = lv_roller_create(win, NULL);
    lv_roller_set_action(obj, select_action);
    lv_group_add_obj(g, obj);

    obj = lv_btnm_create(win, NULL);
    lv_obj_set_size(obj, LV_HOR_RES / 2, LV_VER_RES / 3);
    lv_group_add_obj(g, obj);

    lv_obj_t * ta = lv_ta_create(win, NULL);
    lv_ta_set_cursor_type(ta, LV_CURSOR_BLOCK);
    lv_group_add_obj(g, ta);

    obj = lv_kb_create(win, NULL);
    lv_obj_set_size(obj, LV_HOR_RES - LV_DPI, LV_VER_RES / 2);
    lv_kb_set_ta(obj, ta);
    lv_group_add_obj(g, obj);

    static const char * mbox_btns[] = {"Yes", "No", ""};
    obj = lv_mbox_create(win, NULL);
    lv_mbox_add_btns(obj, mbox_btns, NULL);
    lv_group_add_obj(g, obj);

    obj = lv_list_create(win, NULL);
    lv_list_add(obj, SYMBOL_FILE, "File 1", click_action);
    lv_list_add(obj, SYMBOL_FILE, "File 2", click_action);
    lv_list_add(obj, SYMBOL_FILE, "File 3", click_action);
    lv_list_add(obj, SYMBOL_FILE, "File 4", click_action);
    lv_list_add(obj, SYMBOL_FILE, "File 5", click_action);
    lv_list_add(obj, SYMBOL_FILE, "File 6", click_action);
    lv_group_add_obj(g, obj);

    obj = lv_page_create(win, NULL);
    lv_obj_set_size(obj, 2 * LV_DPI, LV_DPI);
    lv_page_set_arrow_scroll(obj, true);
    lv_group_add_obj(g, obj);
    obj = lv_label_create(obj, NULL);
    lv_label_set_text(obj, "I'm a page\nwith a long \ntext.\n\n"
                      "You can try \nto scroll me\nwith UP and DOWN\nbuttons.");
    lv_label_set_align(obj, LV_LABEL_ALIGN_CENTER);
    lv_obj_align(obj, NULL, LV_ALIGN_CENTER, 0, 0);

    obj = lv_lmeter_create(win, NULL);
    lv_lmeter_set_value(obj, 60);
    lv_group_add_obj(g, obj);

#if LV_COMPILER_NON_CONST_INIT_SUPPORTED
    static lv_color_t needle_color[] = {LV_COLOR_RED};
#else
    static lv_color_t needle_color[] = { 0 };
#endif
    obj = lv_gauge_create(win, NULL);
    lv_gauge_set_needle_count(obj, 1, needle_color);
    lv_gauge_set_value(obj, 0, 80);
    lv_group_add_obj(g, obj);

    obj = lv_chart_create(win, NULL);
    lv_chart_series_t * ser = lv_chart_add_series(obj, LV_COLOR_RED);
    lv_chart_set_next(obj, ser, 40);
    lv_chart_set_next(obj, ser, 30);
    lv_chart_set_next(obj, ser, 35);
    lv_chart_set_next(obj, ser, 50);
    lv_chart_set_next(obj, ser, 60);
    lv_chart_set_next(obj, ser, 75);
    lv_chart_set_next(obj, ser, 80);
    lv_group_add_obj(g, obj);

    obj = lv_led_create(win, NULL);
    lv_group_add_obj(g, obj);

    obj = lv_tabview_create(win, NULL);
    lv_obj_set_size(obj, LV_HOR_RES / 2, LV_VER_RES / 2);
    lv_tabview_add_tab(obj, "Tab 1");
    lv_tabview_add_tab(obj, "Tab 2");
    lv_group_add_obj(g, obj);

    lv_group_focus_obj(lv_group_get_focused(g));
}


/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Read function for the input device which emulates keys on the window header
 * @param data store the last key and its staee here
 * @return false because the reading in not buffered
 */
static bool win_btn_read(lv_indev_data_t * data)
{
    data->state = last_key_state;
    data->key = last_key;


    return false;
}

/**
 * Called when a control button on the window header is pressed to change the key state to PRESSED
 * @param btn pointer t to a button on the window header
 * @return LV_RES_OK  because the button is not deleted
 */
static lv_res_t win_btn_press(lv_obj_t * btn)
{
    LV_OBJ_FREE_NUM_TYPE c = lv_obj_get_free_num(btn);
    last_key_state = LV_INDEV_STATE_PR;
    last_key = c;

    return LV_RES_OK;
}

/**
 * Called when a control button on the window header is released to change the key state to RELEASED
 * @param btn pointer t to a button on the window header
 * @return LV_RES_OK  because the button is not deleted
 */
static lv_res_t win_btn_click(lv_obj_t * btn)
{
    (void) btn; /*Unused*/
    last_key_state = LV_INDEV_STATE_REL;

    return LV_RES_OK;
}


static void group_focus_cb(lv_group_t * group)
{
    lv_obj_t * f = lv_group_get_focused(group);
    if(f != win) lv_win_focus(win, f, 200);
}

/*
 * Dummy action functions
 */

static lv_res_t press_action(lv_obj_t * btn)
{
    (void) btn; /*Unused*/

#if LV_EX_PRINTF
    printf("Press\n");
#endif
    return LV_RES_OK;
}

static lv_res_t select_action(lv_obj_t * btn)
{
    (void) btn; /*Unused*/

#if LV_EX_PRINTF
    printf("Select\n");
#endif
    return LV_RES_OK;
}

static lv_res_t change_action(lv_obj_t * btn)
{
    (void) btn; /*Unused*/

#if LV_EX_PRINTF
    printf("Change\n");
#endif
    return LV_RES_OK;
}

static lv_res_t click_action(lv_obj_t * btn)
{
    (void) btn; /*Unused*/

#if LV_EX_PRINTF
    printf("Click\n");
#endif
    return LV_RES_OK;
}

static lv_res_t long_press_action(lv_obj_t * btn)
{
    (void) btn; /*Unused*/

#if LV_EX_PRINTF
    printf("Long press\n");
#endif
    return LV_RES_OK;
}


#endif /* USE_LV_GROUP && USE_LV_TESTS */
