/*
 * Advanced Logger
 *
 * Copyright 2020 José Antonio Jiménez Campos (@RavenSystem)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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

int adv_logger_init(const uint8_t log_type);

#ifdef __cplusplus
}
#endif

#endif  // __ADV_LOGGER_H__
