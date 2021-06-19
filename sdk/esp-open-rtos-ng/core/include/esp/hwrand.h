/** esp/hwrand.h
 *
 * Hardware Random Number Generator functions.
 *
 * For documentation, see http://esp8266-re.foogod.com/wiki/Random_Number_Generator
 *
 * Part of esp-open-rtos
 * Copyright (C) 2015 Angus Gratton
 * BSD Licensed as described in the file LICENSE
 */
#ifndef _ESP_RNG_H
#define _ESP_RNG_H
#include <stdint.h>
#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* Return a random 32-bit number */
uint32_t hwrand(void);

/* Fill a variable size buffer with data from the Hardware RNG */
void hwrand_fill(uint8_t *buf, size_t len);

#ifdef	__cplusplus
}
#endif

#endif
