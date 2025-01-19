/*
 * Advanced ESP Logger
 *
 * Copyright 2020-2025 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#ifndef __ADV_LOGGER_H__
#define __ADV_LOGGER_H__

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

// Use    nc -kulnw0 45678    to collect UDP network output

void adv_logger_init();
void adv_logger_close_buffered_task();

#ifdef ESP_PLATFORM
int adv_logger_printf(const char* format, ...);
void adv_logger_set_ip_address(char* ip_address);
#endif

#ifdef ADV_LOGGER_REMOVE
#ifndef ESP_PLATFORM
int adv_logger_remove();
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif  // __ADV_LOGGER_H__
