/*
 * Binary Ping
 *
 * Copyright 2020 José A. Jiménez (@RavenSystem)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Based on ping helper example by @maxgerhardt.
 * https://github.com/maxgerhardt/esp-open-rtos-ping-example
 */

#ifndef __BINARY_PING__
#define __BINARY_PING__

#include "lwip/ip_addr.h"

bool ping(ip_addr_t ping_addr);

#endif // __BINARY_PING__
