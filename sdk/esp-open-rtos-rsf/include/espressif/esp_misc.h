/*
 *  Copyright (C) 2013 -2014  Espressif System
 *
 */

#ifndef __ESP_MISC_H__
#define __ESP_MISC_H__

#include "lwip/ip_addr.h"

#ifdef	__cplusplus
extern "C" {
#endif

#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"

#define IP2STR(ipaddr) ip4_addr1_16(ipaddr), \
    ip4_addr2_16(ipaddr), \
    ip4_addr3_16(ipaddr), \
    ip4_addr4_16(ipaddr)

#define IPSTR "%d.%d.%d.%d"

void sdk_os_delay_us(uint16_t us);

void sdk_os_install_putc1(void (*p)(char c));
void sdk_os_putc(char c);

void sdk_gpio_output_set(uint32_t set_mask, uint32_t clear_mask, uint32_t enable_mask, uint32_t disable_mask);

#ifdef	__cplusplus
}
#endif

#endif
