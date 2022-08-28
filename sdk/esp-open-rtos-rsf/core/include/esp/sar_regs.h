/* esp/sar_regs.h
 *
 * ESP8266 register definitions for the "sar" region (0x3FF2xxx)
 *
 * The 0x60000D00 register region is referred to as "sar" by some old header
 * files. Apparently referenced both by ROM I2C functions as well as ADC
 * config/read functions.
 *
 * Not compatible with ESP SDK register access code.
 */

#ifndef _ESP_SAR_REGS_H
#define _ESP_SAR_REGS_H

#include "esp/types.h"
#include "common_macros.h"

#define SAR_BASE 0x60000d00
// Unfortunately,
// esp-open-sdk/xtensa-lx106-elf/xtensa-lx106-elf/sysroot/usr/include/xtensa/config/specreg.h
// already has a "SAR" macro definition which would conflict with this, so
// we'll use "ESPSAR" instead..
#define ESPSAR (*(struct SAR_REGS *)(SAR_BASE))

/* Note: This memory region is not currently well understood.  Pretty much all
 * of the definitions here are from reverse-engineering the Espressif SDK code,
 * many are just educated guesses, and almost certainly some are misleading or
 * wrong.  If you can improve on any of this, please contribute!
 */

struct SAR_REGS {
    uint32_t volatile _unknown0[18];   // 0x00 - 0x44
    uint32_t volatile UNKNOWN_48;      // 0x48 : used by sdk_system_restart_in_nmi()
} __attribute__ (( packed ));

_Static_assert(sizeof(struct SAR_REGS) == 0x4c, "SAR_REGS is the wrong size");

#endif /* _ESP_SAR_REGS_H */

