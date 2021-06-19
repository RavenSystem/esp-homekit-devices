/*
 *  Copyright (C) 2013 -2014  Espressif System
 *
 *
 * Adapted from BSD licensed esp_iot_rtos_sdk v0.9.9
 */

#ifndef __ESP_STA_H__
#define __ESP_STA_H__

#include "queue.h"

#ifdef	__cplusplus
extern "C" {
#endif

struct sdk_station_config {
    uint8_t ssid[32];     /* Null terminated string */
    uint8_t password[64]; /* Null terminated string */
    uint8_t bssid_set;    /* One if bssid is used, otherwise zero. */
    uint8_t bssid[6];     /* The BSSID bytes */
};

bool sdk_wifi_station_get_config(struct sdk_station_config *config);
bool sdk_wifi_station_set_config(struct sdk_station_config *config);

bool sdk_wifi_station_connect(void);
bool sdk_wifi_station_disconnect(void);

struct sdk_scan_config {
    uint8_t *ssid;
    uint8_t *bssid;
    uint8_t channel;
    uint8_t show_hidden;
};

struct sdk_bss_info {
    STAILQ_ENTRY(sdk_bss_info)     next;

    uint8_t bssid[6];
    uint8_t ssid[32];
    uint8_t channel;
    int8_t rssi;
    AUTH_MODE authmode;
    uint8_t is_hidden;
};

/* NB: in esp_iot_rtos_sdk this enum is just called STATUS and has no SCAN_ prefixes */
typedef enum {
    SCAN_OK = 0,
    SCAN_FAIL,
    SCAN_PENDING,
    SCAN_BUSY,
    SCAN_CANCEL,
} sdk_scan_status_t;

typedef void (* sdk_scan_done_cb_t)(void *arg, sdk_scan_status_t status);

bool sdk_wifi_station_scan(struct sdk_scan_config *config, sdk_scan_done_cb_t cb);

uint8_t sdk_wifi_station_get_auto_connect(void);
bool sdk_wifi_station_set_auto_connect(uint8_t set);

enum {
    STATION_IDLE = 0,
    STATION_CONNECTING,
    STATION_WRONG_PASSWORD,
    STATION_NO_AP_FOUND,
    STATION_CONNECT_FAIL,
    STATION_GOT_IP
};

uint8_t sdk_wifi_station_get_connect_status(void);

#ifdef	__cplusplus
}
#endif

#endif
