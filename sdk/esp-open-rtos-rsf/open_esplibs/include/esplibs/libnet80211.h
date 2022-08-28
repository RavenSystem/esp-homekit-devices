/* Internal function declarations for Espressif SDK libnet80211 functions.

   These are internal-facing declarations, it is not recommended to include these headers in your program.
   (look at the headers in include/espressif/ instead and use these whenever possible.)

   Copyright (C) 2015 Espressif Systems. Derived from MIT Licensed SDK libraries.
   BSD Licensed as described in the file LICENSE.
*/

#ifndef _ESPLIBS_LIBNET80211_H
#define _ESPLIBS_LIBNET80211_H

#include "sdk_internal.h"

// ieee80211_action.o

// ieee80211_crypto_ccmp.o
extern uint32_t sdk_ccmp;

// ieee80211_crypto.o

// ieee80211_crypto_tkip.o
extern uint32_t sdk_tkip;

// ieee80211_crypto_wep.o
extern uint32_t sdk_wep;

// ieee80211_ets.o
struct esf_buf *sdk_ieee80211_getmgtframe(void **arg0, uint32_t arg1, uint32_t arg2);

// ieee80211_hostap.o
extern uint8_t sdk_TmpSTAAPCloseAP;
extern uint8_t sdk_PendFreeBcnEb;
void sdk_ieee80211_hostap_attach(struct sdk_g_ic_st *);
void sdk_hostap_handle_timer(struct sdk_cnx_node *cnx_node);
bool sdk_wifi_softap_start();
bool sdk_wifi_softap_stop();

// ieee80211_ht.o

// ieee80211_input.o
void sdk_ieee80211_deliver_data(struct sdk_g_ic_netif_info *netif_info, struct esf_buf *esf_buf);
// The esf_buf is stored in the pbuf->eb slot.
void sdk_ieee80211_deliver_data(struct sdk_g_ic_netif_info *netif_info, struct esf_buf *eb);

// ieee80211.o
extern struct sdk_g_ic_st sdk_g_ic;
extern uint32_t sdk_xieee80211Queue;
void sdk_ieee80211_ifattach(struct sdk_g_ic_st *, uint8_t *);
void sdk_wifi_mode_set(uint8_t);

// ieee80211_output.o
int8_t sdk_ieee80211_output_pbuf(struct netif *ifp, struct pbuf* pb);
void sdk_ieee80211_send_mgmt(struct sdk_g_ic_netif_info *info, int, int);
struct esf_buf *sdk_ieee80211_beacon_alloc(struct sdk_g_ic_netif_info *, uint32_t *);

// ieee80211_phy.o
uint32_t sdk_ieee80211_phy_type_get();
void sdk_ieee80211_phy_init(enum sdk_phy_mode);

// ieee80211_power.o
void sdk_ieee80211_pwrsave(void *, struct esf_buf *b);

// ieee80211_proto.o
extern uint8_t sdk_ieee80211_addr_bcast[6];

// ieee80211_scan.o
extern uint32_t sdk_ugScanStruct; // A struct.
extern uint8_t sdk_auth_type;
extern uint16_t sdk_scannum;
void sdk_scan_cancel();

// ieee80211_sta.o
void sdk_ieee80211_sta_new_state(struct sdk_g_ic_st *, int, int);
void sdk_sta_status_set(int status);
bool sdk_wifi_station_start();
bool sdk_wifi_station_stop();

// wl_chm.o
void sdk_chm_set_current_channel(uint32_t *);
int sdk_ieee80211_chan2ieee(int *);
int sdk_chm_check_same_channel();

// wl_cnx.o
extern ETSTimer sdk_sta_con_timer;
extern void *sdk_g_cnx_probe_rc_list_cb;
void sdk_cnx_sta_leave(struct sdk_g_ic_netif_info *netif_info, void *);
struct sdk_cnx_node *sdk_cnx_node_search(uint8_t mac[6]);
void sdk_cnx_node_leave(struct sdk_g_ic_netif_info *netif, struct sdk_cnx_node *conn);
void sdk_cnx_rc_update_state_metric(void *, int, int);
void sdk_cnx_node_remove(struct sdk_cnx_node *cnx_node);
void sdk_cnx_attach(struct sdk_g_ic_st *);

#endif /* _ESPLIBS_LIBNET80211_H */

