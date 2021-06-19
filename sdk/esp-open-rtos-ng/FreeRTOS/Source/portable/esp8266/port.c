/*
 * FreeRTOS Kernel V10.2.0
 * Copyright (C) 2019 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software. If you wish to use our Amazon
 * FreeRTOS name, please do so in a fair use way that does not cause confusion.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */

/*-----------------------------------------------------------
 * Implementation of functions defined in portable.h for ESP8266
 *
 * This is based on the version supplied in esp_iot_rtos_sdk,
 * which is in turn based on the ARM CM3 port.
 *----------------------------------------------------------*/

#include <xtensa/config/core.h>
#include <malloc.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <xtensa_ops.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "xtensa_rtos.h"

unsigned cpu_sr;
char level1_int_disabled;

/* Supervisor stack pointer entry. This is the "high water mark" of
   how far the supervisor stack grew down before task started. Is zero
   before the scheduler starts.

 After the scheduler starts, task stacks are all allocated from the
 heap and FreeRTOS checks for stack overflow.
*/
void *xPortSupervisorStackPointer;

void vAssertCalled(const char * pcFile, unsigned long ulLine)
{
    printf("rtos assert %s %lu\n", pcFile, ulLine);
    abort();
    //for (;;);
}

/*
 * Stack initialization
 */
portSTACK_TYPE *pxPortInitialiseStack( portSTACK_TYPE *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters )
{
    #define SET_STKREG(r,v) sp[(r) >> 2] = (portSTACK_TYPE)(v)
    portSTACK_TYPE *sp, *tp;

    /* Create interrupt stack frame aligned to 16 byte boundary */
    sp = (portSTACK_TYPE*) (((uint32_t)(pxTopOfStack + 1) - XT_CP_SIZE - XT_STK_FRMSZ) & ~0xf);

    /* Clear the entire frame (do not use memset() because we don't depend on C library) */
    for (tp = sp; tp <= pxTopOfStack; ++tp)
        *tp = 0;

    /* Explicitly initialize certain saved registers */
    SET_STKREG( XT_STK_PC,      pxCode                        );  /* task entrypoint                  */
    SET_STKREG( XT_STK_A0,      0                           );  /* to terminate GDB backtrace       */
    SET_STKREG( XT_STK_A1,      (uint32_t)sp + XT_STK_FRMSZ   );  /* physical top of stack frame      */
    SET_STKREG( XT_STK_A2,      pvParameters   );           /* parameters      */
    SET_STKREG( XT_STK_EXIT,    _xt_user_exit               );  /* user exception exit dispatcher   */

    /* Set initial PS to int level 0, EXCM disabled ('rfe' will enable), user mode. */
    SET_STKREG( XT_STK_PS,      PS_UM | PS_EXCM     );
    return sp;
}

static int pending_soft_sv;
static int pending_maclayer_sv;

/*
 * The portYIELD macro calls PendSV with SVC_Software to set a pending interrupt
 * service callback that allows a task switch, and this occur when interrupts
 * are enabled which might be after exiting the critical region below.
 *
 * The wdev NMI calls this function from pp_post() with SVC_MACLayer to set a
 * pending interrupt service callback which flushs the queue of messages that
 * the NMI stashes away. This interrupt will be triggered after the return from
 * the NMI and when interrupts are enabled. The NMI can not touch the FreeRTOS
 * queues itself. The NMI must not touch the interrupt masks so that path must
 * not call vPortEnterCritical and vPortExitCritical.
 */
void IRAM PendSV(enum SVC_ReqType req)
{
    if (req == SVC_Software) {
        vPortEnterCritical();
        pending_soft_sv = 1;
        WSR(BIT(INUM_SOFT), interrupt);
        vPortExitCritical();
    } else if (req == SVC_MACLayer) {
        pending_maclayer_sv= 1;
        WSR(BIT(INUM_SOFT), interrupt);
    }
}

/* This MAC layer ISR handler is defined in libpp.a, and is called
 * after a Blob SV requests a soft interrupt by calling
 * PendSV(SVC_MACLayer).
 */
extern portBASE_TYPE sdk_MacIsrSigPostDefHdl(void);

void IRAM SV_ISR(void *arg)
{
    portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE ;
    if (pending_maclayer_sv) {
        xHigherPriorityTaskWoken = sdk_MacIsrSigPostDefHdl();
        pending_maclayer_sv = 0;
    }
    if (xHigherPriorityTaskWoken || pending_soft_sv) {
        sdk__xt_timer_int1();
        pending_soft_sv = 0;
    }
}

void xPortSysTickHandle (void)
{
    if (xTaskIncrementTick() != pdFALSE) {
        vTaskSwitchContext();
    }
}

/*
 * See header file for description.
 */
portBASE_TYPE xPortStartScheduler( void )
{
    _xt_isr_attach(INUM_SOFT, SV_ISR, NULL);
    _xt_isr_unmask(BIT(INUM_SOFT));

    /* Initialize system tick timer interrupt and schedule the first tick. */
    _xt_isr_attach(INUM_TICK, sdk__xt_timer_int, NULL);
    _xt_isr_unmask(BIT(INUM_TICK));
    sdk__xt_tick_timer_init();

    vTaskSwitchContext();

    /* mark the supervisor stack pointer high water mark. xt_int_exit
       actually frees ~0x50 bytes off the stack, so this value is
       conservative.
    */
    __asm__ __volatile__ ("mov %0, a1\n" : "=a"(xPortSupervisorStackPointer));

    sdk__xt_int_exit();

    /* Should not get here as the tasks are now running! */
    return pdTRUE;
}

/* Determine free heap size via libc sbrk function & mallinfo

   sbrk gives total size in totally unallocated memory,
   mallinfo.fordblks gives free space inside area dedicated to heap.

   mallinfo is possibly non-portable, although glibc & newlib both support
   the fordblks member.
*/
size_t xPortGetFreeHeapSize( void )
{
    struct mallinfo mi = mallinfo();
    uint32_t brk_val = (uint32_t) sbrk(0);

    intptr_t sp = (intptr_t)xPortSupervisorStackPointer;
    if (sp == 0) {
        /* scheduler not started */
        SP(sp);
    }
    return sp - brk_val + mi.fordblks;
}

void vPortEndScheduler( void )
{
    /* No-op, nothing to return to */
}

/*-----------------------------------------------------------*/

static unsigned portBASE_TYPE uxCriticalNesting = 0;

/* These nested vPortEnter/ExitCritical macros are called by SDK
 * libraries in libmain, libnet80211, libpp
 *
 * It may be possible to replace the global nesting count variable
 * with a save/restore of interrupt level, although it's difficult as
 * the functions have no return value.
 *
 * These should not be called from the NMI in regular operation and
 * the NMI must not touch the interrupt mask, but that might occur in
 * exceptional paths such as aborts and debug code.
 */
void IRAM vPortEnterCritical(void) {
    portDISABLE_INTERRUPTS();
    uxCriticalNesting++;
}

/*-----------------------------------------------------------*/

void IRAM vPortExitCritical(void) {
    uxCriticalNesting--;
    if (uxCriticalNesting == 0)
        portENABLE_INTERRUPTS();
}

/* Backward compatibility, for the sdk library. */

signed portBASE_TYPE xTaskGenericCreate(TaskFunction_t pxTaskCode,
                                        const signed char * const pcName,
                                        unsigned short usStackDepth,
                                        void *pvParameters,
                                        unsigned portBASE_TYPE uxPriority,
                                        TaskHandle_t *pxCreatedTask,
                                        portSTACK_TYPE *puxStackBuffer,
                                        const MemoryRegion_t * const xRegions) {
    (void)puxStackBuffer;
    (void)xRegions;
    return xTaskCreate(pxTaskCode, (const char * const)pcName, usStackDepth,
                       pvParameters, uxPriority, pxCreatedTask);
}

BaseType_t xQueueGenericReceive(QueueHandle_t xQueue, void * const pvBuffer,
                                TickType_t xTicksToWait, const BaseType_t xJustPeeking) {
    configASSERT(xJustPeeking == 0);
    return xQueueReceive(xQueue, pvBuffer, xTicksToWait);
}
