/* Recreated Espressif libwpa wpa_main.o contents.

   Copyright (C) 2015 Espressif Systems. Derived from MIT Licensed SDK libraries.
   BSD Licensed as described in the file LICENSE
*/

#include <strings.h>
#include <string.h>
#include "espressif/user_interface.h"
#include "etstimer.h"
#include "espressif/osapi.h"
#include "espressif/esp_sta.h"
#include "esplibs/libnet80211.h"
#include "esplibs/libmain.h"
#include "esplibs/libwpa.h"
#include "esplibs/libpp.h"
#include "lwip/dhcp.h"
#include "esp/rtcmem_regs.h"

static void wpa_callback1(struct pbuf* pb) {
    struct netif *netif = sdk_g_ic.v.station_netif_info->netif;
    sdk_ieee80211_output_pbuf(netif, pb);
}

static void wpa_callback2(int arg0) {
    struct sdk_g_ic_netif_info *netif_info = sdk_g_ic.v.station_netif_info;
    sdk_ieee80211_send_mgmt(netif_info, 192, arg0);
    sdk_ieee80211_sta_new_state(&sdk_g_ic, 2, (arg0 << 8) | 192);
}

void sdk_wpa_config_profile(struct sdk_g_ic_st *g_ic) {
    uint8_t v = g_ic->s._unknown209;

    if (v == 2 || v == 3 || v == 6) {
        sdk_wpa_set_profile(1);
        return;
    }

    if (v == 4 || v == 5 || v == 7)
        sdk_wpa_set_profile(2);
}

void sdk_wpa_config_bss(struct sdk_g_ic_st *g_ic, uint8_t (* hwaddr2)[6]) {
    struct sdk_g_ic_netif_info *netif_info = g_ic->v.station_netif_info;
    struct netif *netif = netif_info->netif;
    sdk_wpa_set_bss(netif->hwaddr, hwaddr2, g_ic->s._unknown20a, g_ic->s._unknown20c,
                    g_ic->s.sta_password, g_ic->s.sta_ssid.ssid,
                    g_ic->s.sta_ssid.ssid_length);
}

void sdk_wpa_config_assoc_ie(int arg0, int16_t *arg1, int32_t arg2) {
    if (arg0 != 1)
        sdk_g_ic.v._unknown178 = arg1;
    else
        sdk_g_ic.v._unknown174 = arg1;

    *arg1 = arg2;
}

void sdk_dhcp_bind_check(void *parg) {
    struct sdk_g_ic_netif_info *netif_info = sdk_g_ic.v.station_netif_info;
    uint8_t connect_status = netif_info->connect_status;
    uint8_t unknown20a = sdk_g_ic.s._unknown20a;

    if (connect_status != STATION_GOT_IP) {
        if (unknown20a == 7 || unknown20a == 8) {
            netif_info->connect_status = STATION_CONNECTING;
        }
    }
}

void sdk_eagle_auth_done() {
    struct sdk_g_ic_netif_info *netif_info = sdk_g_ic.v.station_netif_info;
    struct netif *netif = netif_info->netif;
    struct sdk_cnx_node *cnx_node = netif_info->_unknown88;

    if (cnx_node->_unknown08 & 1)
        return;

    uint32_t channel = cnx_node->channel->num;
    char *ssid = (char *)sdk_g_ic.s.sta_ssid.ssid;
    printf("\nconnected with %s, channel %d\n", ssid, channel);

    RTCMEM_SYSTEM[61] = 0x00010000 | channel;

    ETSTimer *timer = &netif_info->timer;
    sdk_os_timer_disarm(timer);
    sdk_os_timer_setfn(timer, sdk_dhcp_bind_check, 0);
    sdk_os_timer_arm(timer, 15000, 0);

    netif_info->statusb9 = 0;
    cnx_node->_unknown18 = 0;
    cnx_node->_unknown08 |= 1;

    if (dhcp_supplied_address(netif))
        return;

    if (sdk_dhcpc_flag != DHCP_STOPPED) {
        printf("dhcp client start...\n");
        LOCK_TCPIP_CORE();
        netif_set_up(netif);
        dhcp_start(netif);
        UNLOCK_TCPIP_CORE();
        return;
    }

    if (ip4_addr_isany_val(sdk_info.sta_ipaddr)) {
        printf("expected a static ip address?\n");
        return;
    }

    LOCK_TCPIP_CORE();
    netif_set_addr(netif, &sdk_info.sta_ipaddr, &sdk_info.sta_netmask, &sdk_info.sta_gw);
    netif_set_up(netif);
    UNLOCK_TCPIP_CORE();
    sdk_system_station_got_ip_set(ip_2_ip4(&netif->ip_addr),
                                  ip_2_ip4(&netif->netmask),
                                  ip_2_ip4(&netif->gw));
}

void sdk_wpa_neg_complete() {
    sdk_eagle_auth_done();
}

void sdk_wpa_attach(struct sdk_g_ic_st *g_ic) {
    g_ic->v._unknown180 = NULL;
    g_ic->v._unknown184 = &(g_ic->v._unknown180);
    sdk_wpa_register(0, wpa_callback1, sdk_wpa_config_assoc_ie, sdk_ppInstallKey,
                     wpa_callback2, sdk_wpa_neg_complete);
    sdk_ppRegisterTxCallback(sdk_eapol_txcb, 3);
}
