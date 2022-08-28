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

#ifndef PORTMACRO_H
#define PORTMACRO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp8266.h"
#include "espressif/esp8266/ets_sys.h"
#include <stdint.h>
#include "xtensa_rtos.h"
#include <esp/interrupts.h>

/*-----------------------------------------------------------
 * Port specific definitions for ESP8266
 *
 * The settings in this file configure FreeRTOS correctly for the
 * given hardware and compiler.
 *
 * These settings should not be altered.
 *-----------------------------------------------------------
 */

/* Type definitions. */
#define portCHAR                char
#define portFLOAT               float
#define portDOUBLE              double
#define portLONG                long
#define portSHORT               short
#define portSTACK_TYPE          uint32_t
#define portBASE_TYPE           long

typedef portSTACK_TYPE StackType_t;
typedef portBASE_TYPE BaseType_t;
typedef unsigned portBASE_TYPE UBaseType_t;

typedef uint32_t TickType_t;
#define portMAX_DELAY ( TickType_t ) 0xffffffffUL

/* Architecture specifics. */
#define portARCH_NAME				"ESP8266"
#define portSTACK_GROWTH			( -1 )
#define portTICK_PERIOD_MS			( ( TickType_t ) 1000 / configTICK_RATE_HZ )
#define portBYTE_ALIGNMENT			8
/*-----------------------------------------------------------*/

enum SVC_ReqType {
  SVC_Software = 1,
  SVC_MACLayer = 2,
};

/* Scheduler utilities. */
void PendSV(enum SVC_ReqType);
#define portYIELD()	PendSV(SVC_Software)

/* Task utilities. */
#define portEND_SWITCHING_ISR( xSwitchRequired )			\
    {									\
	extern void vTaskSwitchContext( void );				\
									\
	if( xSwitchRequired )						\
	{								\
	    vTaskSwitchContext();					\
	}								\
    }

/*-----------------------------------------------------------*/

/* NMIIrqIsOn flag is defined in libpp.a, and appears to be set when an NMI
   (int level 3) is currently runnning (during which time libpp.a might
   call back into parts of the OS?)

   The esp_iot_rtos_sdk disables all interrupt manipulations while this
   flag is set.

   It's also referenced from some other blob libraries (not known if
   read or written there).

   ESPTODO: It may be possible to just read the 'ps' register instead
   of accessing thisvariable.
*/
extern uint8_t sdk_NMIIrqIsOn;
extern char level1_int_disabled;
extern unsigned cpu_sr;

/* Disable interrupts, store old ps level in global variable cpu_sr.

   Note: cpu_sr is also referenced by the binary SDK.

   Where possible (and when writing non-FreeRTOS specific code),
   prefer to _xt_disable_interrupts & _xt_enable_interrupts and store
   the ps value in a local variable - that approach is recursive-safe
   and generally better.

   The NMI must not touch the interrupt mask and it should not in
   regular operation, but there is a guard here just in case.
*/
inline static __attribute__((always_inline)) void portDISABLE_INTERRUPTS(void)
{
    if(!sdk_NMIIrqIsOn && !level1_int_disabled) {
	cpu_sr = _xt_disable_interrupts();
	level1_int_disabled = 1;
    }
}

inline static __attribute__((always_inline)) void portENABLE_INTERRUPTS(void)
{
    if(!sdk_NMIIrqIsOn && level1_int_disabled) {
	level1_int_disabled = 0;
        _xt_restore_interrupts(cpu_sr);
    }
}

/* Critical section management. */
void vPortEnterCritical( void );
void vPortExitCritical( void );

#define portENTER_CRITICAL()                vPortEnterCritical()
#define portEXIT_CRITICAL()                 vPortExitCritical()

/* Task function macros as described on the FreeRTOS.org WEB site.  These are
not necessary for to use this port.  They are defined so the common demo files
(which build with all the ports) will build. */
#define portTASK_FUNCTION_PROTO( vFunction, pvParameters ) void vFunction( void *pvParameters )
#define portTASK_FUNCTION( vFunction, pvParameters ) void vFunction( void *pvParameters )

/* FreeRTOS API functions should not be called from the NMI handler. */
#define portASSERT_IF_INTERRUPT_PRIORITY_INVALID() configASSERT(sdk_NMIIrqIsOn == 0)

/*-----------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* PORTMACRO_H */

