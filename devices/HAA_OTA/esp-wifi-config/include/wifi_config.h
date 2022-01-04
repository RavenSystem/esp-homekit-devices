#pragma once

void wifi_config_init(const char *ssid_prefix, TaskHandle_t xHandle);
void wifi_config_remove_sys_param();
