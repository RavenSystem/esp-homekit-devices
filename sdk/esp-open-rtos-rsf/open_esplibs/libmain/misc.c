/* Recreated Espressif libmain misc.o contents.

   Copyright (C) 2015 Espressif Systems. Derived from MIT Licensed SDK libraries.
   BSD Licensed as described in the file LICENSE
*/
#include "open_esplibs.h"
#if OPEN_LIBMAIN_MISC
// The contents of this file are only built if OPEN_LIBMAIN_MISC is set to true

#include "espressif/esp_misc.h"
#include "esp/gpio_regs.h"
#include "esp/rtc_regs.h"
#include "sdk_internal.h"
#include "xtensa_ops.h"

static int cpu_freq = 80;

void (*sdk__putc1)(char);

int IRAM sdk_os_get_cpu_frequency(void) {
    return cpu_freq;
}

void sdk_os_update_cpu_frequency(int freq) {
    cpu_freq = freq;
}

void sdk_ets_update_cpu_frequency(int freq) __attribute__ (( alias ("sdk_os_update_cpu_frequency") ));

void sdk_os_delay_us(uint16_t us) {
    uint32_t start_ccount, ccount;
    uint32_t delay_ccount = cpu_freq * us;

    RSR(start_ccount, ccount);

    do {
        RSR(ccount, ccount);
    } while (ccount - start_ccount < delay_ccount);
}

void sdk_ets_delay_us(uint16_t us) __attribute__ (( alias ("sdk_os_delay_us") ));

void sdk_os_install_putc1(void (*p)(char)) {
    sdk__putc1 = p;
}

void sdk_os_putc(char c) {
    sdk__putc1(c);
}

void sdk_gpio_output_set(uint32_t set_mask, uint32_t clear_mask, uint32_t enable_mask, uint32_t disable_mask) {
    GPIO.OUT_SET = set_mask;
    GPIO.OUT_CLEAR = clear_mask;
    GPIO.ENABLE_OUT_SET = enable_mask;
    GPIO.ENABLE_OUT_CLEAR = disable_mask;
}

uint8_t sdk_rtc_get_reset_reason(void) {
    uint8_t reason;

    reason = FIELD2VAL(RTC_RESET_REASON1_CODE, RTC.RESET_REASON1);
    if (reason == 5) {
        if (FIELD2VAL(RTC_RESET_REASON2_CODE, RTC.RESET_REASON2) == 1) {
            reason = 6;
        } else {
            if (FIELD2VAL(RTC_RESET_REASON2_CODE, RTC.RESET_REASON2) != 8) {
                reason = 0;
            }
        }
    }
    RTC.RESET_REASON0 &= ~RTC_RESET_REASON0_BIT21;
    return reason;
}

#endif /* OPEN_LIBMAIN_MISC */
