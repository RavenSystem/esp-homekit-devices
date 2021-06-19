/* Recreated Espressif libmain os_cpu_o contents.

   Copyright (C) 2015 Espressif Systems. Derived from MIT Licensed SDK libraries.
   BSD Licensed as described in the file LICENSE
*/
#include "open_esplibs.h"
#if OPEN_LIBMAIN_OS_CPU_A
// The contents of this file are only built if OPEN_LIBMAIN_OS_CPU_A is set to true

#include "esp/types.h"
#include "FreeRTOS.h"
#include "task.h"
#include "xtensa_ops.h"
#include "common_macros.h"
#include "esplibs/libmain.h"

// xPortSysTickHandle is defined in FreeRTOS/Source/portable/esp8266/port.c but
// does not exist in any header files.
void xPortSysTickHandle(void);

/* The following "functions" manipulate the stack at a low level and thus cannot be coded directly in C */

void IRAM vPortYield(void) {
    asm("   wsr    a0, excsave1            \n\
            addi   sp, sp, -80             \n\
            s32i   a0, sp, 4               \n\
            addi   a0, sp, 80              \n\
            s32i   a0, sp, 16              \n\
            rsr    a0, ps                  \n\
            s32i   a0, sp, 8               \n\
            rsr    a0, excsave1            \n\
            s32i   a0, sp, 12              \n\
            movi   a0, _xt_user_exit       \n\
            s32i   a0, sp, 0               \n\
            call0  sdk__xt_int_enter       \n\
            call0  vPortEnterCritical      \n\
            call0  vTaskSwitchContext      \n\
            call0  vPortExitCritical       \n\
            call0  sdk__xt_int_exit        \n\
    ");
}

void IRAM sdk__xt_int_enter(void) {
    asm("   s32i   a12, sp, 60             \n\
            s32i   a13, sp, 64             \n\
            mov    a12, a0                 \n\
            call0  sdk__xt_context_save    \n\
            movi   a0, pxCurrentTCB        \n\
            l32i   a0, a0, 0               \n\
            s32i   sp, a0, 0               \n\
            mov    a0, a12                 \n\
    ");
}

void IRAM sdk__xt_int_exit(void) {
    asm("   s32i   a14, sp, 68             \n\
            s32i   a15, sp, 72             \n\
            movi   sp, pxCurrentTCB        \n\
            l32i   sp, sp, 0               \n\
            l32i   sp, sp, 0               \n\
            movi   a14, pxCurrentTCB       \n\
            l32i   a14, a14, 0             \n\
            addi   a15, sp, 80             \n\
            s32i   a15, a14, 0             \n\
            call0  sdk__xt_context_restore \n\
            l32i   a14, sp, 68             \n\
            l32i   a15, sp, 72             \n\
            l32i   a0, sp, 0               \n\
    ");
}

void IRAM sdk__xt_timer_int(void *arg) {
    uint32_t trigger_ccount;
    uint32_t current_ccount;
    uint32_t ccount_interval = portTICK_PERIOD_MS * sdk_os_get_cpu_frequency() * 1000;

    do {
        RSR(trigger_ccount, ccompare0);
        WSR(trigger_ccount + ccount_interval, ccompare0);
        ESYNC();
        xPortSysTickHandle();
        ESYNC();
        RSR(current_ccount, ccount);
    } while (current_ccount - trigger_ccount > ccount_interval);
}

void IRAM sdk__xt_timer_int1(void) {
    vTaskSwitchContext();
}

#define INTENABLE_CCOMPARE  BIT(6)

void IRAM sdk__xt_tick_timer_init(void) {
    uint32_t ints_enabled;
    uint32_t current_ccount;
    uint32_t ccount_interval = portTICK_PERIOD_MS * sdk_os_get_cpu_frequency() * 1000;

    RSR(current_ccount, ccount);
    WSR(current_ccount + ccount_interval, ccompare0);
    ints_enabled = 0;
    XSR(ints_enabled, intenable);
    WSR(ints_enabled | INTENABLE_CCOMPARE, intenable);
}

void IRAM sdk__xt_isr_unmask(uint32_t mask) {
    uint32_t ints_enabled;

    ints_enabled = 0;
    XSR(ints_enabled, intenable);
    WSR(ints_enabled | mask, intenable);
}

void IRAM sdk__xt_isr_mask(uint32_t mask) {
    uint32_t ints_enabled;

    ints_enabled = 0;
    XSR(ints_enabled, intenable);
    WSR(ints_enabled & mask, intenable);
}

uint32_t IRAM sdk__xt_read_ints(void) {
    uint32_t ints_enabled;

    RSR(ints_enabled, intenable);
    return ints_enabled;
}

void IRAM sdk__xt_clear_ints(uint32_t mask) {
    WSR(mask, intclear);
}

#endif /* OPEN_LIBMAIN_OS_CPU_A */
