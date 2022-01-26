/*
 * Advanced ESP Logger with NTP support
 *
 * Copyright 2020-2022 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#ifndef __ADV_LOGGER_H__
#define __ADV_LOGGER_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Max buffer lenth is 1472
#ifndef ADV_ESP_LOG_BUFFER
#define ADV_ESP_LOG_BUFFER      (512)
#endif

// Use    nc -kulnw0 45678    to collect UDP network output

#define ADV_LOGGER_NONE                 (0)
#define ADV_LOGGER_UART0                (1)
#define ADV_LOGGER_UART1                (2)
#define ADV_LOGGER_UDP                  (3)
#define ADV_LOGGER_UART0_UDP            (4)
#define ADV_LOGGER_UART1_UDP            (5)
#define ADV_LOGGER_UDP_BUFFERED         (6)
#define ADV_LOGGER_UART0_UDP_BUFFERED   (7)
#define ADV_LOGGER_UART1_UDP_BUFFERED   (8)


void adv_logger_init(const int log_type, char* dest_addr);

int adv_logger_remove();

#ifdef __cplusplus
}
#endif

#endif  // __ADV_LOGGER_H__
