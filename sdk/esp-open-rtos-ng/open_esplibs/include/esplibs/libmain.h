/* Internal function declarations for Espressif SDK libmain functions.

   These are internal-facing declarations, it is not recommended to include these headers in your program.
   (look at the headers in include/espressif/ instead and use these whenever possible.)

   Copyright (C) 2015 Espressif Systems. Derived from MIT Licensed SDK libraries.
   BSD Licensed as described in the file LICENSE.
*/

#ifndef _ESPLIBS_LIBMAIN_H
#define _ESPLIBS_LIBMAIN_H

#include "sdk_internal.h"

// app_main.o
extern uint8_t sdk_user_init_flag;
extern struct sdk_info_st sdk_info;

// ets_timer.o
extern uint32_t sdk_debug_timer;
extern void *sdk_debug_timerfn;
void sdk_ets_timer_init(void);

// misc.c
int sdk_os_get_cpu_frequency(void);
/* Don't call this function from user code, it doesn't change the CPU
 * speed. Call sdk_system_update_cpu_freq() instead. */
void sdk_os_update_cpu_frequency(int freq);

// os_cpu_a.o

// spi_flash.o
extern sdk_flashchip_t sdk_flashchip;
sdk_SpiFlashOpResult sdk_SPIRead(uint32_t src_addr, uint32_t *des_addr, uint32_t size);
sdk_SpiFlashOpResult sdk_SPIWrite(uint32_t des_addr, uint32_t *src_addr, uint32_t size);

// timers.o
void sdk_os_timer_setfn(ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg);
void sdk_os_timer_arm(ETSTimer *ptimer, uint32_t milliseconds, bool repeat_flag);
void sdk_os_timer_disarm(ETSTimer *ptimer);

// uart.o
void sdk_uart_div_modify(uint32_t uart_no, uint32_t new_divisor);

// user_interface.o
extern enum sdk_dhcp_status sdk_dhcpc_flag; // uint8_t in the sdk
extern bool sdk_cpu_overclock;
extern struct sdk_rst_info sdk_rst_if;
extern sdk_wifi_promiscuous_cb_t sdk_promiscuous_cb;
void sdk_system_restart_in_nmi(void);
int sdk_system_get_test_result(void);
void sdk_wifi_param_save_protect(struct sdk_g_ic_saved_st *data);
bool sdk_system_overclock(void);
bool sdk_system_restoreclock(void);
uint32_t sdk_system_relative_time(uint32_t reltime);
uint32_t sdk_system_get_checksum(uint8_t *, uint32_t);
void sdk_wifi_softap_cacl_mac(uint8_t *, uint8_t *);
void sdk_wifi_softap_set_default_ssid(void);
bool sdk_wifi_softap_set_station_info(const uint8_t *hwaddr, ip4_addr_t *);

// xtensa_context.o

#endif /* _ESPLIBS_LIBMAIN_H */

