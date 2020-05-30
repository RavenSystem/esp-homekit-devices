/*
 * Binary Ping
 *
 * Copyright 2020 José Antonio Jiménez Campos (@RavenSystem)
 *
 */


#ifndef __BINARY_PING__
#define __BINARY_PING__

#include "lwip/ip_addr.h"

bool ping(ip_addr_t ping_addr);

#endif // __BINARY_PING__
