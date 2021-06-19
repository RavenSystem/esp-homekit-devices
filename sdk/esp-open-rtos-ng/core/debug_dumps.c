/* Code for dumping status/debug output/etc, including fatal
 * exception handling & abort implementation.
 *
 * Part of esp-open-rtos
 *
 * Partially reverse engineered from MIT licensed Espressif RTOS SDK Copyright (C) Espressif Systems.
 * Additions Copyright (C) 2015 Superhouse Automation Pty Ltd
 * BSD Licensed as described in the file LICENSE
 */
#include <string.h>
#include <stdio.h>
#include <FreeRTOS.h>
#include <task.h>
#include <malloc.h>
#include <unistd.h>

#include "debug_dumps.h"
#include "common_macros.h"
#include "xtensa_ops.h"
#include "esp/rom.h"
#include "esp/uart.h"
#include "esp/dport_regs.h"
#include "espressif/esp_common.h"
#include "esplibs/libmain.h"
#include "user_exception.h"

/* Forward declarations */
static void IRAM fatal_handler_prelude(void);
/* Inner parts of crash handlers */
typedef void __attribute__((noreturn)) (*fatal_exception_handler_fn)(uint32_t *sp, bool registers_saved_on_stack);
static void __attribute__((noreturn)) standard_fatal_exception_handler_inner(uint32_t *sp, bool registers_saved_on_stack);
static void __attribute__((noreturn)) second_fatal_exception_handler_inner(uint32_t *sp, bool registers_saved_on_stack);
static void __attribute__((noinline)) __attribute__((noreturn)) abort_handler_inner(uint32_t *caller, uint32_t *sp);

static IRAM_DATA fatal_exception_handler_fn fatal_exception_handler_inner = standard_fatal_exception_handler_inner;

static void (*user_exception_handler)(void) = NULL;

/* fatal_exception_handler called from any unhandled user exception
 *
 * (similar to a hard fault on other processor architectures)
 *
 * This function is run from IRAM, but the majority of the handler
 * runs from flash after fatal_handler_prelude ensures it is mapped
 * safely.
 */
void IRAM __attribute__((noreturn)) fatal_exception_handler(uint32_t *sp, bool registers_saved_on_stack) {
    fatal_handler_prelude();
    fatal_exception_handler_fn inner_fn = fatal_exception_handler_inner;
    inner_fn(sp, registers_saved_on_stack);
}

/* Abort implementation
 *
 * Replaces the weak-linked abort implementation provided by newlib libc.
 *
 * Disable interrupts, enable flash mapping, dump stack & caller
 * address, restart.
 *
 * This function is run from IRAM, but the majority of the abort
 * handler runs from flash after fatal_handler_prelude ensures it is
 * mapped safely.
 *
 */
void IRAM abort(void) {
    uint32_t *sp, *caller;
    RETADDR(caller);
    /* abort() caller is one instruction before our return address */
    caller = (uint32_t *)((intptr_t)caller - 3);
    SP(sp);
    fatal_handler_prelude();
    abort_handler_inner(caller, sp);
}

/* Dump exception information from special function registers */
static void dump_excinfo(void) {
    uint32_t exccause, epc1, epc2, epc3, excvaddr, depc, excsave1;
    uint32_t excinfo[8];

    RSR(exccause, exccause);
    printf("Fatal exception (%d): \n", (int)exccause);
    RSR(epc1, epc1);
    RSR(epc2, epc2);
    RSR(epc3, epc3);
    RSR(excvaddr, excvaddr);
    RSR(depc, depc);
    RSR(excsave1, excsave1);
    printf("%s=0x%08x\n", "epc1", epc1);
    printf("%s=0x%08x\n", "epc2", epc2);
    printf("%s=0x%08x\n", "epc3", epc3);
    printf("%s=0x%08x\n", "excvaddr", excvaddr);
    printf("%s=0x%08x\n", "depc", depc);
    printf("%s=0x%08x\n", "excsave1", excsave1);
    sdk_system_rtc_mem_read(0, excinfo, 32); // Why?
    excinfo[0] = 2;
    excinfo[1] = exccause;
    excinfo[2] = epc1;
    excinfo[3] = epc2;
    excinfo[4] = epc3;
    excinfo[5] = excvaddr;
    excinfo[6] = depc;
    excinfo[7] = excsave1;
    sdk_system_rtc_mem_write(0, excinfo, 32);
}

/* dump stack memory (frames above sp) to stdout

   There's a lot of smart stuff we could do while dumping stack
   but for now we just dump what looks like our stack region.

   Probably dumps more memory than it needs to, the first instance of
   0xa5a5a5a5 probably constitutes the end of our stack.
*/
void dump_stack(uint32_t *sp) {
    printf("\nStack: SP=%p\n", sp);
    for(uint32_t *p = sp; p < sp + 32; p += 4) {
        if((intptr_t)p >= 0x3fffc000) {
            break; /* approximate end of RAM */
        }
        printf("%p: %08x %08x %08x %08x\n", p, p[0], p[1], p[2], p[3]);
        if(p[0] == 0xa5a5a5a5 && p[1] == 0xa5a5a5a5
           && p[2] == 0xa5a5a5a5 && p[3] == 0xa5a5a5a5) {
            break; /* FreeRTOS uses this pattern to mark untouched stack space */
        }
    }
}

/* Dump normal registers that were stored above 'sp'
   by the exception handler preamble
*/
void dump_registers_in_exception_handler(uint32_t *sp) {
    uint32_t excsave1;
    uint32_t *saved = sp - (0x50 / sizeof(uint32_t));
    printf("Registers:\n");
    RSR(excsave1, excsave1);
    printf("a0 %08x ", excsave1);
    printf("a1 %08x ", (intptr_t)sp);
    for(int a = 2; a < 14; a++) {
        printf("a%-2d %08x%c", a, saved[a+3], a == 3 || a == 7 || a == 11 ? '\n':' ');
    }
    printf("SAR %08x\n", saved[0x13]);
}

static void __attribute__((noreturn)) post_crash_reset(void) {
    uart_flush_txfifo(0);
    uart_flush_txfifo(1);
    sdk_system_restart_in_nmi();
    while(1) {}
}

/* Prelude ensures exceptions/NMI off and flash is mapped, allowing
   calls to non-IRAM functions.
*/
static void IRAM fatal_handler_prelude(void) {
    if (!sdk_NMIIrqIsOn) {
        vPortEnterCritical();
        do {
            DPORT.DPORT0 &= 0xffffffe0;
        } while (DPORT.DPORT0 & 0x00000001);
    }
    Cache_Read_Disable();
    Cache_Read_Enable(0, 0, 1);

    if (user_exception_handler != NULL) {
      user_exception_handler();
    }
}

/* Main part of fatal exception handler, is run from flash to save
   some IRAM.
*/
static void standard_fatal_exception_handler_inner(uint32_t *sp, bool registers_saved_on_stack) {
    /* Replace the fatal exception handler 'inner' function so we
       don't end up in a crash loop if this handler crashes. */
    fatal_exception_handler_inner = second_fatal_exception_handler_inner;
    dump_excinfo();
    if (sp) {
        if (registers_saved_on_stack) {
            dump_registers_in_exception_handler(sp);
        }
        dump_stack(sp);
    }
    dump_heapinfo();
    post_crash_reset();
}

/* This is the exception handler that gets called if a crash occurs inside the standard handler,
   so we don't end up in a crash loop. It doesn't rely on contents of stack or heap.
*/
static void second_fatal_exception_handler_inner(uint32_t *sp, bool registers_saved_on_stack) {
    dump_excinfo();
    printf("Second fatal exception occured inside fatal exception handler. Can't continue.\n");
    post_crash_reset();
}

void dump_heapinfo(void)
{
    extern char _heap_start;
    extern uint32_t xPortSupervisorStackPointer;
    struct mallinfo mi = mallinfo();
    uint32_t brk_val = (uint32_t) sbrk(0);
    uint32_t sp = xPortSupervisorStackPointer;
    if(sp == 0) {
        SP(sp);
    }

    /* Total free heap is all memory that could be allocated via
       malloc (assuming fragmentation doesn't become a problem) */
    printf("\nFree Heap: %d\n", sp - brk_val + mi.fordblks);

    /* delta between brk & supervisor sp is the contiguous memory
       region that is available to be put into heap space via
       brk(). */
    printf("_heap_start %p brk 0x%08x supervisor sp 0x%08x sp-brk %d bytes\n",
           &_heap_start, brk_val, sp, sp-brk_val);

    /* arena/fordblks/uordblks determines the amount of free space
      inside the heap region already added via brk(). May be
      fragmented.

       The values in parentheses are the values used internally by
       nano-mallocr.c, the field names outside parentheses are the
       POSIX compliant field names of the mallinfo structure.

       "arena" should be equal to brk-_heap_start ie total size available.
     */
    printf("arena (total_size) %d fordblks (free_size) %d uordblocks (used_size) %d\n",
           mi.arena, mi.fordblks, mi.uordblks);
}

/* Main part of abort handler, can be run from flash to save some
   IRAM.
*/
static void abort_handler_inner(uint32_t *caller, uint32_t *sp) {
    printf("abort() invoked at %p.\n", caller);
    dump_stack(sp);
    dump_heapinfo();
    post_crash_reset();
}

void set_user_exception_handler(void (*fn)(void))
{
  user_exception_handler = fn;
}

