#pragma once

void wifi_config_smart_connect();
void wifi_config_reset();
int wifi_config_get_ip();
uint32_t wifi_config_get_full_gw();

#ifdef ESP_PLATFORM

void wifi_config_init(const char* ssid_prefix, void (*on_wifi_ready)(), const char* custom_hostname, const int param, const uint8_t wifi_sleep_mode);
void setup_set_esp_netif(esp_netif_t* esp_net_if);
esp_netif_t* setup_get_esp_netif();
void setup_set_boot_installer();
uint8_t wifi_config_connect(const uint8_t mode, const bool with_reset);
#define save_last_working_phy()

#else

void wifi_config_init(const char* ssid_prefix, void (*on_wifi_ready)(), const char* custom_hostname, const int param);
uint8_t wifi_config_connect(const uint8_t mode, const uint8_t phy, const bool with_reset);
void save_last_working_phy();

#endif


