/* ESP8266 "Hardware RNG" (validity still being confirmed) support for ESP8266
 *
 * Based on research done by @foogod.
 *
 * Please don't rely on this too much as an entropy source, quite yet...
 *
 * Part of esp-open-rtos
 * Copyright (C) 2015 Angus Gratton
 * BSD Licensed as described in the file LICENSE
 */
#include <mbedtls/entropy_poll.h>
#include <esp/hwrand.h>

int mbedtls_hardware_poll( void *data,
                           unsigned char *output, size_t len, size_t *olen )
{
    (void)(data);
    hwrand_fill(output, len);
    if(olen)
        *olen = len;
    return 0;
}
