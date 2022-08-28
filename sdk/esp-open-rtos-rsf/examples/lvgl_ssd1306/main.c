#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <timers.h>
#include <string.h>
#include "lv_conf.h"
#include "lvgl/lvgl.h"
#include "lv_drivers/display/SSD1306.h"

#define I2C_CONNECTION false
#define TICK_HANDLER   false
#define OVERCLOCK      false

#define TICK_INC_MS (10)
#define RST_PIN (LV_DRIVER_NOPIN)
#define SECOND (1000 / portTICK_PERIOD_MS)

#if (I2C_CONNECTION)
#include <i2c/i2c.h>
#endif

#if (I2C_CONNECTION)
#define PROTOCOL SSD1306_PROTO_I2C
#define ADDR     SSD1306_I2C_ADDR_0
#define I2C_BUS  0
#define SCL_PIN  5
#define SDA_PIN  4

const static i2c_dev_t i2c_dev =
{
    .bus = I2C_BUS,
    .addr = ADDR
};
#else
#define PROTOCOL SSD1306_PROTO_SPI3
#define SPI_BUS  1
#define CS_PIN   15
#define DC_PIN   4
/*Delare bus descriptor */
const static lv_spi_dev_t spi_dev =
{
    .bus = SPI_BUS,
    .cs = CS_PIN,
    .dc = DC_PIN,
};
#endif

/* Declare device descriptor */
static ssd1306_t dev;

TimerHandle_t fps_timer_handle = NULL; // Timer handler
TimerHandle_t font_timer_handle = NULL;
TimerHandle_t lvgl_timer_handle = NULL;

static lv_style_t style;
static lv_obj_t * label;

static void ssd1306_task(void *pvParameters)
{
    printf("%s: Started user interface task\n", __FUNCTION__);
    vTaskDelay(SECOND);
    ssd1306_set_whole_display_lighting(&dev, false);

    //Set a style for the obj
    lv_style_copy(&style, &lv_style_transp);
    style.text.font = &lv_font_dejavu_10;   /*Unicode and symbol fonts already assigned by the library*/
    style.text.color.full = 1;
    style.text.opa = 255;

    style.body.main_color.full = 0;
    style.body.grad_color.full = 0;
    style.body.shadow.color.full = 0;
    style.body.border.color.full = 0;
    style.body.empty = 1;

    style.image.color.full = 1;
    style.image.intense = 255;
    style.image.opa = 255;

    style.line.color.full = 1;
    style.line.opa = 255;
    style.line.width = 1;
    style.line.rounded = false;

    //Create main screen obj
    lv_obj_t * scr = lv_obj_create(NULL, NULL);
    lv_scr_load(scr);
    lv_obj_set_style(scr, &style);

    //Create a simple label
    label = lv_label_create(lv_scr_act(), NULL);
    lv_obj_set_style(label, &style);

    lv_obj_align(label, lv_scr_act(), LV_ALIGN_IN_TOP_LEFT, 0, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_BREAK);
    lv_label_set_align(label, LV_LABEL_ALIGN_CENTER);
    lv_label_set_text(label, "lvgl work with esp-open-rtos");
    lv_obj_set_width(label, LV_HOR_RES);

    while (1) {
        /*draw system call */
        lv_task_handler();
        vTaskDelay(1);
    }
}


void font_timer(TimerHandle_t h)
{
    static uint8_t index = 0;
    if (index >= 2)
        index=0;
    else
        index++;

    switch (index) {
    case 0:
        style.text.font = &lv_font_dejavu_10;
        break;
    case 1:
        style.text.font = &lv_font_dejavu_20;
        break;
    case 2:
        style.text.font = &lv_font_dejavu_30;
        break;
    default:
        break;
    }
    lv_label_set_style(label, &style);
    printf("Selected builtin font %d\n", index);
}


#if (!LV_TICK_CUSTOM)
#if (TICK_HANDLER)
void IRAM vApplicationTickHook(void) {
    lv_tick_inc(portTICK_PERIOD_MS);
}
#else
void lvgl_timer(TimerHandle_t xTimerHandle)
{
    lv_tick_inc(TICK_INC_MS);
}
#endif
#endif

void user_init(void)
{
#if (OVERCLOCK)
    sdk_system_update_cpu_freq(160);
#endif

    // Setup HW
    uart_set_baud(0, 115200);
    printf("SDK version:%s\n", sdk_system_get_sdk_version());

#if (I2C_CONNECTION)
    i2c_init(I2C_BUS, SCL_PIN, SDA_PIN, I2C_FREQ_400K);
#else
#if (CS_PIN == 15)
    spi_init(SPI_BUS, SPI_MODE0, SPI_FREQ_DIV_8M, true, SPI_LITTLE_ENDIAN, false);
#else
    spi_init(SPI_BUS, SPI_MODE0, SPI_FREQ_DIV_8M, true, SPI_LITTLE_ENDIAN, true);
    gpio_enable(CS_PIN, GPIO_OUTPUT);
#endif
#endif
#if (RST_PIN != LV_DRIVER_NOPIN)
    gpio_enable(RST_PIN, GPIO_OUTPUT);
#endif

    /*Gui initialization*/
    lv_init();

    /*Screen initialization*/
#if (I2C_CONNECTION)
    dev.i2c_dev = (lv_i2c_handle_t)&i2c_dev;
#else
    dev.spi_dev = (lv_spi_handle_t)&spi_dev;
#endif
    dev.protocol = PROTOCOL ; //SSD1306_PROTO_SPI3;
    dev.screen   = SH1106_SCREEN; //SSD1306_SCREEN,  SH1106_SCREEN
    dev.width    = LV_HOR_RES;
    dev.height   = LV_VER_RES;
    dev.rst_pin  = RST_PIN; //No RST PIN USED

    while (ssd1306_init(&dev) != 0) {
        printf("%s: failed to init SSD1306 lcd\n", __func__);
        vTaskDelay(SECOND);
    }

    ssd1306_set_whole_display_lighting(&dev, true);

    /*inverse screen (180°) */
    ssd1306_set_scan_direction_fwd(&dev,true);
    ssd1306_set_segment_remapping_enabled(&dev, false);
    vTaskDelay(SECOND);

    /*lvgl screen registration */
    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    /*Set up the functions to access to your display*/
    disp_drv.disp_flush = ssd1306_flush; /*Used in buffered mode (LV_VDB_SIZE != 0  in lv_conf.h)*/
    disp_drv.disp_fill = NULL;//ex_disp_fill;   /*Used in unbuffered mode (LV_VDB_SIZE == 0  in lv_conf.h)*/
    disp_drv.disp_map = NULL;//ex_disp_map;    /*Used in unbuffered mode (LV_VDB_SIZE == 0  in lv_conf.h)*/
    disp_drv.vdb_wr = ssd1306_vdb_wr;
    lv_disp_drv_register(&disp_drv);

    /* Create user interface task */
    xTaskCreate(ssd1306_task, "ssd1306_task", 512, NULL, 2, NULL);

    font_timer_handle = xTimerCreate("font_timer", 5 * SECOND, pdTRUE, NULL, font_timer);
    xTimerStart(font_timer_handle, 0);

#if (!TICK_HANDLER && !LV_TICK_CUSTOM)
    lvgl_timer_handle = xTimerCreate("lvgl_timer", TICK_INC_MS/portTICK_PERIOD_MS, pdTRUE, NULL, lvgl_timer);
    xTimerStart(lvgl_timer_handle, 0);
#endif
}
