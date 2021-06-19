/*
 *  Copyright (C) 2013 -2014  Espressif System
 *
 */

#ifndef __ESP_SOFTAP_H__
#define __ESP_SOFTAP_H__

#ifdef	__cplusplus
extern "C" {
#endif

struct sdk_softap_config {
    uint8_t ssid[32];
    uint8_t password[64];
    uint8_t ssid_len;
    uint8_t channel;
    AUTH_MODE authmode;
    uint8_t ssid_hidden;
    uint8_t max_connection;
    uint16_t beacon_interval;
};

bool sdk_wifi_softap_get_config(struct sdk_softap_config *config);
bool sdk_wifi_softap_set_config(struct sdk_softap_config *config);

#ifdef	__cplusplus
}
#endif

#endif
