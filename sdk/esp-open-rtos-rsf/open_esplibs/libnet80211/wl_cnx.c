/* Recreated Espressif libnet80211 wl_cnx.o contents.

   Copyright (C) 2015 Espressif Systems. Derived from MIT Licensed SDK libraries.
   BSD Licensed as described in the file LICENSE
*/

#include "espressif/esp_misc.h"
#include "esplibs/libnet80211.h"
#include "esplibs/libpp.h"
#include "esplibs/libwpa.h"
#include <string.h>
#include "lwip/dhcp.h"

/* Need to use the sdk versions of these for now as there are reference to them
 * relative to other data structres. */
extern ETSTimer sdk_sta_con_timer;
extern void *sdk_g_cnx_probe_rc_list_cb;

/*
 * Called from the ESP sdk_cnx_sta_leave function. Split out via a hack to the
 * binary library to allow modification to track changes to lwip, for example
 * changes to the offset of the netif->flags removal of the NETIF_FLAG_DHCP flag
 * lwip v2 etc.
 */
void dhcp_if_down(struct netif *netif)
{
    LOCK_TCPIP_CORE();
    dhcp_release_and_stop(netif);
    netif_set_down(netif);
    UNLOCK_TCPIP_CORE();
}

struct sdk_cnx_node *sdk_cnx_rc_search(uint8_t *hwaddr) {
    size_t len = *(uint8_t *)(sdk_g_ic.v._unknown0 + 0x689);
    struct sdk_cnx_node **table = (struct sdk_cnx_node **)(sdk_g_ic.v._unknown0 + 0x670);
    size_t i;

    for (i = 0; i < len; i++) {
        struct sdk_cnx_node *cnx_node = table[i];
        if (cnx_node && memcmp(cnx_node->mac_addr, hwaddr, 6) == 0) {
            return cnx_node;
        }
    }

    return NULL;
}

int sdk_cnx_add_rc(struct sdk_cnx_node *cnx_node) {
    size_t len = *(uint8_t *)(sdk_g_ic.v._unknown0 + 0x689);
    struct sdk_cnx_node **table = (struct sdk_cnx_node **)(sdk_g_ic.v._unknown0 + 0x670);

    if (len >= 6) {
        return -1;
    }

    if (len < 2) {
        table[len] = cnx_node;
    } else {
        struct wl_channel *channel = cnx_node->channel;
        size_t found;
        for (found = 0; found < len; found++) {
            if (table[found]->channel == channel) {
                break;
            }
        }

        if (found >= len) {
            /* Add to the end. */
            table[len] = cnx_node;
        } else {
            /* Make room. */
            size_t next = found + 1;
            size_t i;
            for (i = len; i > next; i--) {
                table[i] = table[i - 1];;
            }
            table[next] = cnx_node;
        }
    }

    *(uint8_t *)(sdk_g_ic.v._unknown0 + 0x689) += 1;
    return 0;
}

void sdk_cnx_remove_rc(struct sdk_cnx_node *cnx_node) {
    size_t len = *(uint8_t *)(sdk_g_ic.v._unknown0 + 0x689);
    struct sdk_cnx_node **table = (struct sdk_cnx_node **)(sdk_g_ic.v._unknown0 + 0x670);
    size_t i;

    for (i = 0; i < len; i++) {
        if (table[i] == cnx_node) {
            bzero(cnx_node, 0x110);
            table[i] = NULL;
            len -= 1;
            *(uint8_t *)(sdk_g_ic.v._unknown0 + 0x689) = len;
            break;
        }
    }

    /* Fill the hole */
    for (; i < len; i++) {
        table[i] = table[i + 1];
    }
}

#if 0

// Most of the code in this file assesses static data so it will be all or none.
static uint32_t Ldata001;
static uint8_t Ldata003;
static uint8_t Ldata004;
static uint32_t Ldata006;
static void *Ldate007;

void sdk_cnx_sta_leave(struct sdk_g_ic_netif_info *netif_info, void *arg1) {
    struct netif *netif = netif_info->netif;

    uint32_t phy_type = sdk_ieee80211_phy_type_get();
    uint16_t v1 = *(uint16_t *)(arg1 + 0x1a) & 0xfff;
    sdk_ic_set_sta(0, 0, arg1, 0, v1, phy_type, 0, 0);

    // Note the SDK binary was modified here as it made use of the
    // netif flags which changed in lwip v2.
    dhcp_if_down(netif);

    uint32_t v2 = *(uint8_t *)(arg1 + 0xe8);
    free(sdk_g_ic.v._unknown190[v2]);
    sdk_g_ic.v._unknown190[v2] = NULL;

    if (sdk_g_ic.v._unknown190[0]) {
        free(sdk_g_ic.v._unknown190[0]);
        sdk_g_ic.v._unknown190[0] = NULL;
    }

    if (sdk_g_ic.v._unknown190[1]) {
        free(sdk_g_ic.v._unknown190[1]);
        sdk_g_ic.v._unknown190[1] = NULL;
    }

    sdk_scan_cancel();

    sdk_wDev_SetRxPolicy(0, 0, 0);

    Ldata001 = 2;

    uint8_t v3 = *(uint8_t *)(arg1 + 0x6);
    if (v3 & 2) {
        *(uint8_t *)(arg1 + 0x6) = v3 & 0xfd;
        sdk_cnx_rc_update_state_metric(arg1, -7, 1);
        Ldate007 = arg1;
    }

    sdk_cnx_remove_rc(arg1);

    Ldata006 = 0;
    *(uint32_t *)(arg1 + 0x8) &= 0xfffffffe;
    netif_info->_unknown88 = NULL;

    return;
}
#endif

void sdk_cnx_node_remove(struct sdk_cnx_node *cnx_node) {
    const uint32_t num = sdk_g_ic.s._unknown310 + 2;
    if ((int32_t)num < (int32_t)2) {
        return;
    }

    struct sdk_g_ic_netif_info *netif_info = sdk_g_ic.v.softap_netif_info;
    uint32_t i = 1;
    do {
        if (netif_info->cnx_nodes[i] == cnx_node) {
            uint32_t v2 = cnx_node->_unknowne8;
            sdk_ic_remove_key(v2 + 2);
            sdk_wpa_auth_sta_deinit(cnx_node->_unknowne4);
            free(sdk_g_ic.v._unknown190[v2]);
            sdk_g_ic.v._unknown190[v2] = NULL;
            free(cnx_node);
            netif_info->cnx_nodes[i] = NULL;
            return;
        }
        i += 1;
    } while (i < num);
}

struct sdk_cnx_node *sdk_cnx_node_search(uint8_t mac[6])
{
    int end = sdk_g_ic.s._unknown310 + 2;

    // Note this defensive test seems dead code, the value is loaded
    // as a uint8_t value so adding 2 ensures this test never passes.
    if (end < 1)
        return NULL;

    struct sdk_cnx_node **cnx_nodes = sdk_g_ic.v.softap_netif_info->cnx_nodes;

    /* Multicast addresses */
    if (mac[0] & 0x01) {
        return cnx_nodes[0];
    }

    int i = 0;
    do {
        struct sdk_cnx_node *cnx_node = cnx_nodes[i];

        if (cnx_node) {
            if (memcmp(mac, cnx_node->mac_addr, 6) == 0) {
                return cnx_node;
            }
        }
        i++;
    } while (i < end);

    return NULL;
}
