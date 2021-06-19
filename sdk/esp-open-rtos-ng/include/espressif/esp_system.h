/*
 *  Copyright (C) 2013 -2014  Espressif System
 *
 */

#ifndef __ESP_SYSTEM_H__
#define __ESP_SYSTEM_H__

#ifdef	__cplusplus
extern "C" {
#endif

enum sdk_rst_reason {
	DEFAULT_RST	  = 0,
	WDT_RST	      = 1,
	EXCEPTION_RST = 2,
	SOFT_RST      = 3
};

struct sdk_rst_info{
	uint32_t reason;
	uint32_t exccause;
	uint32_t epc1;
	uint32_t epc2;
	uint32_t epc3;
	uint32_t excvaddr;
	uint32_t depc;
	uint32_t rtn_addr;
};

struct netif *sdk_system_get_netif(uint32_t mode);

struct sdk_rst_info* sdk_system_get_rst_info(void);

const char* sdk_system_get_sdk_version(void);

enum sdk_sleep_type {
    WIFI_SLEEP_NONE = 0,
    WIFI_SLEEP_LIGHT = 1,
    WIFI_SLEEP_MODEM = 2,
};
bool sdk_wifi_set_sleep_type(enum sdk_sleep_type);
enum sdk_sleep_type sdk_wifi_get_sleep_type(void);

void sdk_system_restore(void);
void sdk_system_restart(void);
bool sdk_system_deep_sleep_set_option(uint8_t option);
void sdk_system_deep_sleep(uint32_t time_in_us);

uint32_t sdk_system_get_time(void);

void sdk_system_print_meminfo(void);
uint32_t sdk_system_get_free_heap_size(void);
uint32_t sdk_system_get_chip_id(void);

uint32_t sdk_system_rtc_clock_cali_proc(void);
uint32_t sdk_system_get_rtc_time(void);

bool sdk_system_rtc_mem_read(uint32_t src_addr, void *des_addr, uint16_t save_size);
bool sdk_system_rtc_mem_write(uint32_t des_addr, void *src_addr, uint16_t save_size);

void sdk_system_uart_swap(void);
void sdk_system_uart_de_swap(void);

#define SYS_CPU_80MHZ   80
#define SYS_CPU_160MHZ  160
/*
   Set CPU frequency in MHz. All peripheral devices are clocked by independent
   system bus, and CPU frequency change will not affect them.
 */
bool sdk_system_update_cpu_freq(uint8_t freq);
uint8_t sdk_system_get_cpu_freq(void);

/*
   Measure voltage on the TOUT pin, 1V max. Returns 10 bits ADC value (1/1024V).
   Voltage range 0 .. 1.0V. RF must be enabled.
   Example:
      printf ("ADC voltage is %.3f", 1.0 / 1024 * sdk_system_adc_read());
 */
uint16_t sdk_system_adc_read(void);


#ifdef	__cplusplus
}
#endif

#endif
