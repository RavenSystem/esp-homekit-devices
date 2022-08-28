/* Functions for dumping status/debug output/etc, including fatal
 * exception handling.
 *
 * Part of esp-open-rtos
 *
 * Copyright (C) 2015-2016 Superhouse Automation Pty Ltd
 * BSD Licensed as described in the file LICENSE
 */
#ifndef _DEBUG_DUMPS_H
#define _DEBUG_DUMPS_H
#include <stdint.h>

/* Dump stack memory to stdout, starting from stack pointer address sp. */
void dump_stack(uint32_t *sp);

/* Dump heap statistics to stdout */
void dump_heapinfo(void);

/* Called from exception_vectors.S when a fatal exception occurs.

   Probably not useful to be called in other contexts.
*/
void __attribute__((noreturn)) fatal_exception_handler(uint32_t *sp, bool registers_saved_on_stack);
void __attribute__((weak, alias("fatal_exception_handler")))
	debug_exception_handler(uint32_t *sp, bool registers_saved_on_stack);

#endif
