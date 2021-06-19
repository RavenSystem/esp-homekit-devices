/*
 * FatFs integration to esp-open-rtos
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#ifndef _EXTRAS_FATFS_VOLUMES_H_
#define _EXTRAS_FATFS_VOLUMES_H_

#include <stdint.h>

const char *f_gpio_to_volume(uint8_t gpio);
uint8_t f_drv_to_gpio(uint8_t drv);

#endif /* _EXTRAS_FATFS_VOLUMES_H_ */
