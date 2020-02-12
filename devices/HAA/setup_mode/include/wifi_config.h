#pragma once

void wifi_config_init(const char *ssid_prefix, const char *password, void (*on_wifi_ready)(), const char *custom_hostname);
bool wifi_config_connect();
void wifi_config_reset();
