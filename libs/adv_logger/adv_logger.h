/*
 * Advanced Logger
 *
 * Copyright 2020 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#ifndef __ADV_LOGGER_H__
#define __ADV_LOGGER_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Use       nc -kulnw0 45678 to collect UDP network output

#define ADV_LOGGER_NONE         (0)
#define ADV_LOGGER_UART0        (1)
#define ADV_LOGGER_UART1        (2)
#define ADV_LOGGER_UDP          (3)
#define ADV_LOGGER_UART0_UDP    (4)
#define ADV_LOGGER_UART1_UDP    (5)

void adv_logger_init(const uint8_t log_type);

#ifdef __cplusplus
}
#endif

#endif  // __ADV_LOGGER_H__
