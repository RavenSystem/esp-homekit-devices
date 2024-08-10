#pragma once

void wifi_config_smart_connect();
uint32_t wifi_config_get_full_gw();

#ifdef ESP_PLATFORM

int wifi_config_get_ip();
void wifi_config_init(void (*on_wifi_ready)(), char* custom_hostname, const int param, const uint8_t wifi_sleep_mode, const bool bandwidth_40);
void setup_set_esp_netif(esp_netif_t* esp_net_if);
esp_netif_t* setup_get_esp_netif();
void setup_set_boot_installer();
void wifi_config_connect(const uint8_t mode);
#define save_last_working_phy()

#else

int IRAM wifi_config_get_ip();
void wifi_config_init(void (*on_wifi_ready)(), char* custom_hostname, const int param);
void wifi_config_connect(const uint8_t mode, const uint8_t phy, const bool with_reset);
void wifi_config_reset();
void save_last_working_phy();

#endif


