/* Experiments to dump timer registers at various points, mess around
 * with timer registers.
 *
 * NOT good code, not example code, nothing something you probably
 * want to mess with.
 *
 * This experimental reverse engineering code is in the public domain.
 */
#include "espressif/esp_common.h"
#include "esp/uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "esp8266.h"
#include "common_macros.h"

#define DUMP_SZ 0x10 /* number of regs not size of buffer */

IRAM void dump_frc1_seq(void)
{
    uint32_t f1_a = TIMER(0).COUNT;
    uint32_t f1_b = TIMER(0).COUNT;
    uint32_t f1_c = TIMER(0).COUNT;
    printf("FRC1 sequence 0x%08x 0x%08x 0x%08x\r\n", f1_a, f1_b, f1_c);
    printf("FRC1 deltas %d %d \r\n", f1_b-f1_a, f1_c-f1_b);
}

IRAM void dump_frc2_seq(void)
{
    /* this sequence of reads compiles down to sequence of l32is with
     * memw instructions in between.
     *
     * counts at various divisor values:
     * /1 = 13
     * /16 = 0 or 1 (usually 1)
     * 
     */
    uint32_t f2_a = TIMER(1).COUNT;
    uint32_t f2_b = TIMER(1).COUNT;
    uint32_t f2_c = TIMER(1).COUNT;
    printf("FRC2 sequence 0x%08x 0x%08x 0x%08x\r\n", f2_a, f2_b, f2_c);
    printf("FRC2 deltas %d %d \r\n", f2_b-f2_a, f2_c-f2_b);
}

IRAM void dump_timer_regs(const char *msg)
{
    esp_reg_t reg = (esp_reg_t)TIMER_BASE;
    static uint32_t chunk[DUMP_SZ];

    /* load everything as quickly as possible to get a "snapshot" */
    for(int i = 0; i < DUMP_SZ; i++) {
        chunk[i] = reg[i];
    }

    printf("%s:\r\n", msg);
    /* print the chunk we loaded */
    for(int i = 0; i < DUMP_SZ; i++) {
        if(i % 4 == 0)
            printf("%s0x%02x: ", i ? "\r\n" : "", i*4);
        printf("%08x ", chunk[i]);
    }
    printf("\r\n");

    dump_frc1_seq();
    dump_frc2_seq();
}

extern uint32_t isr[16];
extern uint32_t seen_isr[16];
extern uint32_t max_count;

static volatile uint32_t frc2_handler_call_count;
static volatile uint32_t frc2_last_count_val;

static volatile uint32_t frc1_handler_call_count;
static volatile uint32_t frc1_last_count_val;

void timerRegTask(void *pvParameters)
{
    while(1) {
        printf("state at task tick count %d:\r\n", xTaskGetTickCount());
        dump_timer_regs("");

        /*
        for(int i = 0; i < 16; i++) {
            printf("int 0x%02x: 0x%08x (%d)\r\n", i, isr[i], seen_isr[i]);
        }
        printf("INUM_MAX count %d\r\n", max_count);
        */

        printf("frc1 handler called %d times, last value 0x%08x\r\n", frc1_handler_call_count,
               frc1_last_count_val);

        printf("frc2 handler called %d times, last value 0x%08x\r\n", frc2_handler_call_count,
               frc2_last_count_val);

        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

IRAM void frc1_handler(void *arg)
{
    frc1_handler_call_count++;
    frc1_last_count_val = TIMER(0).COUNT;
    //TIMER(0).LOAD = 0x300000;
    //TIMER(0).STATUS = 0;
    //TIMER_FRC1_MATCH_REG = frc1_last_count_val + 0x100000;
}

void frc2_handler(void *arg)
{
    frc2_handler_call_count++;
    frc2_last_count_val = TIMER(1).COUNT;
    TIMER(1).ALARM = frc2_last_count_val + 0x100000;
    //TIMER(1).LOAD = 0;
    //TIMER(1).LOAD = 0x2000000;
    //TIMER(1).STATUS = 0;
}

void user_init(void)
{
    uart_set_baud(0, 115200);
    xTaskCreate(timerRegTask, "timerRegTask", 1024, NULL, 2, NULL);

    TIMER(0).CTRL = VAL2FIELD(TIMER_CTRL_CLKDIV, TIMER_CLKDIV_256) | TIMER_CTRL_RELOAD;
    TIMER(0).LOAD = 0x200000;

    TIMER(1).LOAD = VAL2FIELD(TIMER_CTRL_CLKDIV, TIMER_CLKDIV_256);

    DPORT.INT_ENABLE |= DPORT_INT_ENABLE_TIMER0 | DPORT_INT_ENABLE_TIMER1;
    _xt_isr_attach(INUM_TIMER_FRC1, frc1_handler, NULL);
    _xt_isr_unmask(1<<INUM_TIMER_FRC1);
    _xt_isr_attach(INUM_TIMER_FRC2, frc2_handler, NULL);
    _xt_isr_unmask(1<<INUM_TIMER_FRC2);

    TIMER(0).CTRL |= TIMER_CTRL_RUN;
    TIMER(1).CTRL |= TIMER_CTRL_RUN;

    dump_timer_regs("timer regs during user_init");
    dump_timer_regs("#2 timer regs during user_init");
    dump_timer_regs("#3 timer regs during user_init");
}
