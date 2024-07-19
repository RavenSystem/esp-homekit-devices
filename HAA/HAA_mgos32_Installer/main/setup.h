#pragma once

void setup_set_esp_netif(esp_netif_t* esp_net_if);
void wifi_config_init(TaskHandle_t xHandle);
void setup_mode_reset_sysparam();
