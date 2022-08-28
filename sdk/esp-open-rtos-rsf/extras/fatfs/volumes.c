/*
 * FatFs integration to esp-open-rtos
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#include "volumes.h"
#include <stddef.h>

const char *volumes[] = {
    [0] = "3:",
    [1] = NULL,
    [2] = "4:",
    [3] = NULL,
    [4] = "2:",
    [5] = "1:",
    [6] = NULL,
    [7] = NULL,
    [8] = NULL,
    [9] = NULL,
    [10] = NULL,
    [11] = NULL,
    [12] = NULL,
    [13] = NULL,
    [14] = NULL,
    [15] = "0:",
    [16] = "5:"
};

const uint8_t pins[] = {
    15, 5, 4, 0, 2, 16
};

const char *f_gpio_to_volume(uint8_t gpio)
{
    return gpio > 16 ? NULL : volumes[gpio];
}

uint8_t f_drv_to_gpio(uint8_t drv)
{
    return pins[drv];
}
