/* Recreated Espressif libwpa is_xtensa.s contents.

   Copyright (C) 2015 Espressif Systems. Derived from MIT Licensed SDK libraries.
   BSD Licensed as described in the file LICENSE
*/
#include "open_esplibs.h"
#if OPEN_LIBWPA_OS_XTENSA
// The contents of this file are only built if OPEN_LIBWPA_WPA_MAIN is set to true

#include <stdlib.h>
#include <string.h>
#include <esplibs/libwpa.h>
#include <common_macros.h>

// Used by wpa_get_ntp_timestamp.
int IRAM sdk_os_get_time(uint32_t time[]) {
    return 0;
}

uint32_t IRAM sdk_os_random() {
    return rand();
}

int IRAM sdk_os_get_random(uint8_t *dst, uint32_t size) {
    uint32_t end = size >> 2;
    if (end > 0) {
        uint32_t i = 0;
        do {
            uint32_t n = rand();
            memcpy(dst, &n, sizeof(n));
            dst += 4;
            i++;
        } while (i < end);
    }
    return 0;
}

#endif /* OPEN_LIBWPA_OS_XTENSA */
