/* Recreated Espressif libnet80211 ieee80211_sta.o contents.

   Copyright (C) 2015 Espressif Systems. Derived from MIT Licensed SDK libraries.
   BSD Licensed as described in the file LICENSE
*/

#include <string.h>
#include "esplibs/libmain.h"
#include "esplibs/libnet80211.h"
#include "esplibs/libpp.h"
#include "esplibs/libwpa.h"
#include "tcpip.h"
#include "espressif/esp_sta.h"


void sdk_sta_status_set(int status) {
    struct sdk_g_ic_netif_info *netif_info = sdk_g_ic.v.station_netif_info;
    uint32_t statusb8 = netif_info->statusb8;

    if (statusb8 == 1 || statusb8 == status) {
        uint32_t statusb9 = netif_info->statusb9 + 1;
        netif_info->statusb9 = statusb9;
        if (statusb9 == 3)
            netif_info->connect_status = status;
    } else {
        netif_info->statusb9 = 0;
        netif_info->connect_status = 1;
    }

    netif_info->statusb8 = status;

    return;
}

bool sdk_wifi_station_start() {
    struct sdk_g_ic_netif_info *netif_info = sdk_g_ic.v.station_netif_info;
    if (!netif_info)
        return 0;

    if (!netif_info->started) {
        if (!netif_info->netif) {
            struct netif *netif = (struct netif *)malloc(sizeof(struct netif));
            netif_info->netif = netif;
            memcpy(&netif->hwaddr, &sdk_info.sta_mac_addr, 6);
            LOCK_TCPIP_CORE();
            netif_add(netif, &sdk_info.sta_ipaddr, &sdk_info.sta_netmask,
                      &sdk_info.sta_gw, netif_info, ethernetif_init, tcpip_input);
            UNLOCK_TCPIP_CORE();
            sdk_wpa_attach(&sdk_g_ic);
        }
        sdk_ic_set_vif(0, 1, &sdk_info.sta_mac_addr, 0, 0);
        netif_info->statusb8 = 0;
        netif_info->statusb9 = 0;
        netif_info->started = 1;
    }

    return 1;
}

bool sdk_wifi_station_stop() {
    struct sdk_g_ic_netif_info *netif_info = sdk_g_ic.v.station_netif_info;
    if (!netif_info)
        return 0;

    if (netif_info->started) {
        netif_info->statusb8 = 0;
        netif_info->statusb9 = 0;
        sdk_wifi_station_disconnect();
        sdk_ic_set_vif(0, 0, NULL, 0, 0);
        netif_info->started = 0;
    }

    return 1;
}
