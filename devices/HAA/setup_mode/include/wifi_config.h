#pragma once

void wifi_config_init(const char *ssid_prefix, const char *password, void (*on_wifi_ready)(), const char *custom_hostname, const int param);
uint8_t wifi_config_connect(const uint8_t mode);
void wifi_config_smart_connect();
void wifi_config_reset();
void wifi_config_resend_arp();
int wifi_config_remove_sys_param();
bool wifi_config_got_ip();
