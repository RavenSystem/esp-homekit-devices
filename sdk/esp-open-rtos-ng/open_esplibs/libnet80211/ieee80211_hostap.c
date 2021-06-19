/* Recreated Espressif libnet80211 ieee80211_hostap.o contents.

   Copyright (C) 2015 Espressif Systems. Derived from MIT Licensed SDK libraries.
   BSD Licensed as described in the file LICENSE
*/

#include <string.h>
#include "tcpip.h"
#include "espressif/esp_wifi.h"
#include "espressif/esp_misc.h"
#include "etstimer.h"
#include "esplibs/libmain.h"
#include "esplibs/libnet80211.h"
#include "esplibs/libpp.h"
#include "esplibs/libwpa.h"

static uint8_t hostap_flags = 0;
static ETSTimer hostap_timer;
static struct esf_buf *hostap_timer_parg = NULL;

void IRAM *zalloc(size_t nbytes);

static void IRAM hostap_timer_func(struct esf_buf *esf_buf) {
    struct sdk_cnx_node *cnx_node = sdk_g_ic.v.softap_netif_info->cnx_nodes[0];
    int32_t mode = sdk_wifi_get_phy_mode();
    uint8_t *frame = esf_buf->frame;
    *(uint16_t *)(frame + 22) = (cnx_node->_unknown9c - 1) << 4;

    cnx_node->_unknown9c += 1;

    if (sdk_g_ic.s.wifi_led_enable) {
        uint32_t gpio = sdk_g_ic.s.wifi_led_gpio;
        uint32_t state = sdk_g_ic.s.wifi_led_state;
        sdk_gpio_output_set(state << gpio, (((state & 1) == 0) ? 1 : 0) << gpio,
                            1 << gpio, 0);
        sdk_g_ic.s.wifi_led_state = (state & 1) ? 0 : 1;
    }

    uint8_t *frame2 = frame + sdk_g_ic.s._unknown288 + (mode == 1 ? 23 : 27) ;
    memcpy(frame2 + 29, &sdk_g_ic.v._unknown1d2, 1);

    uint32_t v1 = frame2[26];
    if (v1 == 0) {
        v1 = frame2[27];
    }
    frame2[26] = v1 - 1;

    uint32_t v2 = sdk_ieee80211_chan2ieee(sdk_g_ic.v._unknown14c);
    frame2[23] = v2;
    sdk_g_ic.s._unknown30d = v2;

    int32_t v3 = *((volatile int32_t *)0x3ff20c00); // mactime
    *(uint32_t *)(esf_buf->extra + 16) = v3;
    *(uint32_t *)(frame + 24) = v3;
    *(uint32_t *)(frame + 28) = 0;

    if (sdk_chm_check_same_channel()) {
        hostap_flags |= 1;
        sdk_ppTxPkt(esf_buf);
        return;
    }
    sdk_ets_timer_disarm(&hostap_timer);
    sdk_ets_timer_arm(&hostap_timer, sdk_wDev_Get_Next_TBTT(), 0);
}

static void IRAM hostap_tx_callback() {
    uint32_t flags = hostap_flags & 0xfe;

    if (sdk_TmpSTAAPCloseAP == 0) {
        hostap_flags = flags;
        sdk_ets_timer_disarm(&hostap_timer);
        sdk_ets_timer_arm(&hostap_timer, sdk_wDev_Get_Next_TBTT(), 0);
        return;
    }

    sdk_PendFreeBcnEb = 1;

    if (flags & 2) {
        hostap_flags = flags & 0xfd;
        sdk_wifi_softap_start();
        return;
    }

    hostap_flags = flags;
}

static void hostap_attach_misc() {
    struct sdk_g_ic_netif_info *netif_info = sdk_g_ic.v.softap_netif_info;

    struct _unknown_softap1 *ptr1 = zalloc(28); // 0x1c
    netif_info->_unknownb4 = ptr1;

    struct _unknown_softap2 *ptr2 = zalloc(204); // 0xcc
    ptr1->_unknown04 = ptr2;

    struct _unknown_wpa1 *ptr3 = zalloc(76); // 0x4c

    uint32_t v1 = sdk_g_ic.s._unknown30e;
    if (v1 == 2) {
        ptr3->_unknown00 = 1;
    } else if (v1 == 3) {
        ptr3->_unknown00 = 2;
    } else if (v1 == 4) {
        ptr3->_unknown00 = 3;
    }

    ptr3->_unknown28 = 2;
    ptr3->_unknown04 = 2;
    ptr3->_unknown0c = 2;
    ptr3->_unknown08 = 10;
    ptr3->_unknown20 = 10;

    int s1 = (sdk_g_ic.s._unknown28a << 16) | sdk_g_ic.s._unknown288;
    memcpy(&ptr2->_unknown10, &sdk_g_ic.s._unknown28c, s1);
    ptr2->_unknown30 = s1;

    uint8_t *ptr4 = zalloc(64); // 0x40
    ptr2->_unknown3c = ptr4;

    char *str1 = sdk_g_ic.s._unknown2ac;
    memcpy(ptr4, str1, strlen(str1));

    struct sdk_cnx_node *cnx_node = netif_info->cnx_nodes[0];

    ptr2->_unknownb4 = 300;

    netif_info->_unknown4c = 3;
    netif_info->_unknown48 |= 16;

    cnx_node->_unknown08 |= 1;

    sdk_hostapd_setup_wpa_psk(ptr2);

    struct netif *netif = netif_info->netif;

    ptr1->_unknown18 = sdk_wpa_init(&netif->hwaddr, ptr3, 0);

    free(ptr3);
}

static void softap_stop_free() {
    struct sdk_g_ic_netif_info *netif_info = sdk_g_ic.v.softap_netif_info;
    netif_info->_unknown4c = 0;
    netif_info->_unknown48 &= 0xffffffef;
    netif_info->cnx_nodes[0]->_unknown08 = 0;

    struct _unknown_softap1 *unkb4 = netif_info->_unknownb4;
    if (!unkb4) return;

    uint32_t *unk18 = unkb4->_unknown18;
    if (unk18) {
        uint32_t *ptr1 = ((uint32_t **)unk18)[20];
        if (ptr1)
            free(ptr1);

        uint32_t *ptr2 = *(uint32_t **)unk18;
        if (ptr2)
            free(ptr2);

        free(unk18);
    }

    struct _unknown_softap2 *unk04 = unkb4->_unknown04;
    if (unk04) {
        uint32_t *unk38 = unk04->_unknown38;
        if (unk38)
            free(unk38);

        uint8_t *unk3c = unk04->_unknown3c;
        if (unk3c)
            free(unk3c);

        free(unk04);
    }

    free(unkb4);
    netif_info->_unknownb4 = NULL;
}

void sdk_ieee80211_hostap_attach(struct sdk_g_ic_st *ic) {
    uint32_t scratch[12]; // ??
    struct sdk_g_ic_netif_info *netif_info = ic->v.softap_netif_info;

    uint32_t v1 = ic->s._unknown30e;
    if (v1 >= 2 && v1 < 5)
        hostap_attach_misc();

    struct netif *netif = netif_info->netif;
    sdk_ic_bss_info_update(1, &netif->hwaddr, 2, 100);

    ic->v._unknown1d0 = 0;
    netif_info->_unknown3c = 5;

    sdk_ppRegisterTxCallback(hostap_tx_callback, 4);

    hostap_timer_parg = sdk_ieee80211_beacon_alloc(netif_info, scratch);
    sdk_ets_timer_disarm(&hostap_timer);
    sdk_ets_timer_setfn(&hostap_timer, (ETSTimerFunc *)hostap_timer_func, hostap_timer_parg);
    sdk_wDev_Reset_TBTT();
    sdk_ets_timer_arm(&hostap_timer, sdk_wDev_Get_Next_TBTT(), 0);
}

bool sdk_wifi_softap_start() {
    struct sdk_g_ic_netif_info *netif_info = sdk_g_ic.v.softap_netif_info;
    if (!netif_info) return 0;
    if (netif_info->started) return 1;

    uint8_t flags = hostap_flags;
    if (flags & 1) {
        hostap_flags = flags | 2;
        return 1;
    }

    uint8_t (*mac_addr)[6]  = &sdk_info.softap_mac_addr;
    if (!netif_info->netif) {
         struct netif *netif = (struct netif *)malloc(sizeof(struct netif));
         netif_info->netif = netif;
         memcpy(&netif->hwaddr, mac_addr, 6);
         LOCK_TCPIP_CORE();
         netif_add(netif, &sdk_info.softap_ipaddr, &sdk_info.softap_netmask,
                   &sdk_info.softap_gw, netif_info, ethernetif_init, tcpip_input);
         UNLOCK_TCPIP_CORE();
     }

    sdk_ic_set_vif(1, 1, mac_addr, 1, 0);

    LOCK_TCPIP_CORE();
    netif_set_up(netif_info->netif);
    UNLOCK_TCPIP_CORE();

    if (sdk_wifi_get_opmode() != 3 ||
        !sdk_g_ic.v.station_netif_info ||
        sdk_g_ic.v.station_netif_info->_unknown3c < 2) {

        uint32_t i1 = (sdk_g_ic.s._unknown30d - 1) & 0xff;

        int nmi_on = sdk_NMIIrqIsOn;
        if (!nmi_on) {
            vPortEnterCritical();

            do {
                DPORT.DPORT0 = DPORT.DPORT0 & 0xffffffe0;
            }  while (DPORT.DPORT0 & 1);
        }

        // current channel?
        uint32_t *chan = &sdk_g_ic.v._unknown84[i1 * 3];
        sdk_g_ic.v._unknown14c = chan;

        if (!nmi_on) {
            DPORT.DPORT0 = (DPORT.DPORT0 & 0xffffffe0) | 1;
            vPortExitCritical();
        }

        sdk_chm_set_current_channel(chan);
    }

    sdk_ieee80211_hostap_attach(&sdk_g_ic);
    sdk_TmpSTAAPCloseAP = 0;
    netif_info->started = 1;
    return 1;
}

bool sdk_wifi_softap_stop() {
    struct sdk_g_ic_netif_info *netif_info = sdk_g_ic.v.softap_netif_info;

    if (!netif_info)
        return 0;

    if (!netif_info->started)
        return 1;

    uint32_t end = sdk_g_ic.s._unknown310 + 2;
    uint32_t count = 1;

    // Note this defensive test seems dead code, the value is loaded
    // as a uint8_t value so adding 2 ensures this test always passes.
    if (end >= 2) {
        do {
            struct sdk_cnx_node *cnx_node = netif_info->cnx_nodes[count];
            if (cnx_node) {
                struct sdk_cnx_node *cnx_node2 = netif_info->_unknown88;
                netif_info->_unknown88 = cnx_node;

                sdk_ieee80211_send_mgmt(netif_info, 160, 4);
                sdk_ieee80211_send_mgmt(netif_info, 192, 2);

                netif_info->_unknown88 = cnx_node2;

                sdk_cnx_node_leave(netif_info, netif_info->cnx_nodes[count]);

                // Number of entries might have changed, perhaps
                // should have if one was removed above?
                end = sdk_g_ic.s._unknown310 + 2;
            }
            count++;
        } while (count < end);
    }

    LOCK_TCPIP_CORE();
    netif_set_down(netif_info->netif);
    UNLOCK_TCPIP_CORE();
    sdk_TmpSTAAPCloseAP = 1;
    sdk_ets_timer_disarm(&hostap_timer);
    sdk_ic_bss_info_update(1, &sdk_info.softap_mac_addr, 2, 0);
    sdk_ic_set_vif(1, 0, NULL, 1, 0);
    softap_stop_free();

    if ((hostap_flags & 1) == 0)
        sdk_esf_buf_recycle(hostap_timer_parg, 4);

    netif_info->started = 0;
    return 1;
}
