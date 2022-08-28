#ifndef __USER_INTERFACE_H__
#define __USER_INTERFACE_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "espressif/esp_wifi.h"

enum sdk_dhcp_status {
    DHCP_STOPPED,
    DHCP_STARTED
};

uint8_t sdk_system_get_boot_version(void);
uint32_t sdk_system_get_userbin_addr(void);
uint8_t sdk_system_get_boot_mode(void);
bool sdk_system_restart_enhance(uint8_t bin_type, uint32_t bin_addr);
bool sdk_system_upgrade_userbin_set(uint8_t userbin);
uint8_t sdk_system_upgrade_userbin_check(void);
bool sdk_system_upgrade_flag_set(uint8_t flag);
uint8_t sdk_system_upgrade_flag_check(void);
bool sdk_system_upgrade_reboot(void);
bool sdk_wifi_station_dhcpc_start(void);
bool sdk_wifi_station_dhcpc_stop(void);
enum sdk_dhcp_status sdk_wifi_station_dhcpc_status(void);

#endif
