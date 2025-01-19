/*
 * Advanced ESP Logger with NTP support
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

#define ADV_LOGGER_NONE                 (0)
#define ADV_LOGGER_UART0                (1)
#define ADV_LOGGER_UART1                (2)
#define ADV_LOGGER_UART2                (3)
#define ADV_LOGGER_UDP                  (4)
#define ADV_LOGGER_UART0_UDP            (5)
#define ADV_LOGGER_UART1_UDP            (6)
#define ADV_LOGGER_UART2_UDP            (7)
#define ADV_LOGGER_UDP_BUFFERED         (8)
#define ADV_LOGGER_UART0_UDP_BUFFERED   (9)
#define ADV_LOGGER_UART1_UDP_BUFFERED   (10)
#define ADV_LOGGER_UART2_UDP_BUFFERED   (11)


void adv_logger_init(const uint8_t log_type, char* dest_addr, const bool with_header);

#ifdef ESP_PLATFORM
int adv_logger_printf(const char* format, ...);
void adv_logger_set_ip_address(char* ip_address);
#endif

#ifdef ADV_LOGGER_REMOVE
int adv_logger_remove();
#endif

#ifdef __cplusplus
}
#endif

#endif  // __ADV_LOGGER_H__
