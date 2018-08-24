#pragma once

void wifi_config_init(const char *ssid_prefix, const char *password, void (*on_wifi_ready)());

void wifi_config_reset();
void wifi_config_get(char **ssid, char **password);
void wifi_config_set(const char *ssid, const char *password);
