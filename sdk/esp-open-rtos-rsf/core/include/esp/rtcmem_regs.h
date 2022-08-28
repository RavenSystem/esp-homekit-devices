/* esp/rtcmem_regs.h
 *
 * ESP8266 RTC semi-persistent memory register definitions
 *
 * Not compatible with ESP SDK register access code.
 */

#ifndef _ESP_RTCMEM_REGS_H
#define _ESP_RTCMEM_REGS_H

#include "esp/types.h"
#include "common_macros.h"

/* The RTC memory is a range of 256 words (1 KB) of general-purpose memory
 * within the Real Time Clock peripheral.  Because it's part of the RTC, it
 * continues to be powered (and retains its contents) even when the ESP8266 is
 * in its deepest sleep mode (and other RAM is lost).  It can therefore be
 * useful for keeping data which must be persisted through sleep or a reset.
 *
 * Note, however, that it is not "battery backed", or flash memory, and thus
 * will not keep its contents if power is removed entirely.
 */

// We could just define these as 'volatile uint32_t *', but doing things this
// way means that the RTCMEM* defines will include array size information, so
// the C compiler can do bounds-checking for static arguments.

typedef volatile uint32_t rtcmem_array64_t[64];
typedef volatile uint32_t rtcmem_array128_t[128];
typedef volatile uint32_t rtcmem_array256_t[256];

#define RTCMEM_BASE 0x60001000

/* RTCMEM is an array covering the entire semi-persistent memory range */
#define RTCMEM (*(rtcmem_array256_t *)(RTCMEM_BASE))

/* RTCMEM_BACKUP / RTCMEM_SYSTEM / RTCMEM_USER are the same range, divided up
 * into chunks by application/use, as defined by Espressif */

#define RTCMEM_BACKUP (*(rtcmem_array64_t *)(RTCMEM_BASE))
#define RTCMEM_SYSTEM (*(rtcmem_array64_t *)(RTCMEM_BASE + 0x100))
#define RTCMEM_USER (*(rtcmem_array128_t *)(RTCMEM_BASE + 0x200))

#endif /* _ESP_RTCMEM_REGS_H */
