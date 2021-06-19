/*
 *  Copyright (C) 2013 -2014  Espressif System
 *
 * Adapted from BSD licensed esp_iot_rtos_sdk v0.9.9
 *
 * Functions declared in this header are defined in libmain.a
 */

#ifndef __ESP_WIFI_H__
#define __ESP_WIFI_H__
#include <stdint.h>
#include <stdbool.h>

#include <lwip/ip_addr.h>

#ifdef	__cplusplus
extern "C" {
#endif

enum {
	NULL_MODE = 0,
	STATION_MODE,
	SOFTAP_MODE,
	STATIONAP_MODE,
	MAX_MODE
};

typedef enum _auth_mode {
    AUTH_OPEN           = 0,
    AUTH_WEP,
    AUTH_WPA_PSK,
    AUTH_WPA2_PSK,
    AUTH_WPA_WPA2_PSK,
    AUTH_MAX
} AUTH_MODE;

uint8_t sdk_wifi_get_opmode(void);
bool sdk_wifi_set_opmode(uint8_t opmode);

enum {
	STATION_IF = 0,
	SOFTAP_IF,
	MAX_IF
};

struct ip_info {
    struct ip4_addr ip;
    struct ip4_addr netmask;
    struct ip4_addr gw;
};

bool sdk_wifi_get_ip_info(uint8_t if_index, struct ip_info *info);
bool sdk_wifi_set_ip_info(uint8_t if_index, struct ip_info *info);
bool sdk_wifi_get_macaddr(uint8_t if_index, uint8_t *macaddr);
bool sdk_wifi_set_macaddr(uint8_t if_index, uint8_t *macaddr);

uint8_t sdk_wifi_get_channel(void);
bool sdk_wifi_set_channel(uint8_t channel);

void sdk_wifi_status_led_install(uint8_t gpio_id, uint32_t gpio_name, uint8_t gpio_func);

void sdk_wifi_promiscuous_enable(uint8_t promiscuous);

typedef void (* sdk_wifi_promiscuous_cb_t)(uint8_t *buf, uint16_t len);

void sdk_wifi_set_promiscuous_rx_cb(sdk_wifi_promiscuous_cb_t cb);

enum sdk_phy_mode {
	PHY_MODE_11B	= 1,
	PHY_MODE_11G	= 2,
	PHY_MODE_11N    = 3
};

enum sdk_phy_mode sdk_wifi_get_phy_mode(void);
bool sdk_wifi_set_phy_mode(enum sdk_phy_mode mode);

#ifdef	__cplusplus
}
#endif

#endif
