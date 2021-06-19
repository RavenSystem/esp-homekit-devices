#ifndef _INTERNAL_SDK_STRUCTURES_H
#define _INTERNAL_SDK_STRUCTURES_H

#include "espressif/esp_wifi.h"
#include "espressif/spi_flash.h"
#include "espressif/phy_info.h"
#include "etstimer.h"
#include "lwip/netif.h"

///////////////////////////////////////////////////////////////////////////////
//                   Internal structures and data objects                    //
///////////////////////////////////////////////////////////////////////////////

// 'info' is declared in app_main.o at .bss+0x4

struct sdk_info_st {
    ip4_addr_t softap_ipaddr;    // 0x00
    ip4_addr_t softap_netmask;   // 0x04
    ip4_addr_t softap_gw;        // 0x08
    ip4_addr_t sta_ipaddr;       // 0x0c
    ip4_addr_t sta_netmask;      // 0x10
    ip4_addr_t sta_gw;           // 0x14
    uint8_t softap_mac_addr[6]; // 0x18
    uint8_t sta_mac_addr[6];    // 0x1e
};


struct wl_channel {
    uint8_t _unknown00;
    uint8_t _unknown01;
    uint8_t _unknown02;
    uint8_t _unknown03;
    uint8_t _unknown04;
    uint8_t _unknown05;
    uint8_t num; // eagle_auth_done
};


struct _unknown_softap2 {
    uint32_t _unknown00;
    uint32_t _unknown04;
    uint32_t _unknown08;
    uint32_t _unknown0c;
    uint32_t _unknown10[8]; // block copied from sdk_g_ic.s._unknown28c
    uint32_t _unknown30;
    uint32_t _unknown34;
    uint32_t *_unknown38;
    uint8_t *_unknown3c; // string copied from sdk_g_ic.s._unknown2ac
    uint32_t _unknown40[29];
    uint32_t _unknownb4; // 300
    uint32_t _unknownb8[5];
};


struct _unknown_softap1 {
    uint32_t _unknown00;
    struct _unknown_softap2 *_unknown04;
    uint32_t _unknown08[4];
    uint32_t *_unknown18; // result of sdk_wpa_init, dynamically allocated object.
};


struct _unknown_wpa1 {
    uint32_t _unknown00; // 1, 2, 3
    uint32_t _unknown04; // 2
    uint32_t _unknown08; // 10
    uint32_t _unknown0c;
    uint32_t _unknown10;
    uint32_t _unknown14;
    uint32_t _unknown18;
    uint32_t _unknown1c;
    uint32_t _unknown20; // 10
    uint32_t _unknown24;
    uint32_t _unknown28;
    uint32_t _unknown2c;
    uint32_t _unknown30;
    uint32_t _unknown34;
    uint32_t _unknown38;
    uint32_t _unknown3c;
    uint32_t _unknown40; // 2
    uint32_t _unknown44;
    uint32_t _unknown48;
};


struct sdk_cnx_node {
    uint8_t mac_addr[6];
    uint8_t _unknown07[2];

    uint32_t _unknown08; // eagle_auth_done

    uint32_t _unknown0c[3];

    int8_t _unknown18; // eagle_auth_done
    int8_t _unknown19;
    int8_t _unknown1a;
    int8_t _unknown1b;

    uint32_t _unknown1c[23];

    struct wl_channel *channel; // 0x78 eagle_auth_done

    uint32_t _unknown7c[8];

    uint16_t _unknown9c; // ieee80211_hostap. increases by one one each timer func called.
    uint16_t _unknown9e;

    uint32_t _unknowna0[17];

    void *_unknowne4;

    uint8_t _unknowne8; //
    uint8_t _unknowne9; // ppInstallKey
    int8_t _unknownea;
    int8_t _unknowneb;

    uint32_t _unknownec[7];

    uint32_t _unknown108; // hostap_handle_timer count
};


struct sdk_g_ic_netif_info {
    struct netif *netif;     // 0x00
    ETSTimer timer;          // 0x04 - 0x20
    uint8_t _unknown20[28];  // 0x20 - 0x3c
    uint32_t _unknown3c;     // 0x3c (referenced by sdk_wifi_station_disconnect)
    uint8_t _unknown40[6];   // 0x40 - 0x46
    uint8_t _unknown46[2];   // 0x46 - 0x47
    uint32_t _unknown48;     // 0x48
    uint8_t _unknown4c;      // 0x4c
    uint8_t _unknown4d[59];  // 0x4d - 0x88
    struct sdk_cnx_node *_unknown88;  // 0x88
    uint32_t _unknown8c;     // 0x8c
    struct sdk_cnx_node *cnx_nodes[6]; // 0x90 - 0xa8
    uint8_t _unknowna8[12];  // 0xa8 - 0xb4
    struct _unknown_softap1 *_unknownb4;
    uint8_t statusb8;        // 0xb8 (arg of sta_status_set)
    uint8_t statusb9;        // 0xb9 (compared to arg of sta_status_set)
    uint8_t connect_status;  // 0xba (result of wifi_station_get_connect_status)
    uint8_t started;         // 0xbb (referenced by sdk_wifi_station_start / sdk_wifi_station_stop)
};


// This is the portion of g_ic which is not loaded/saved to the flash ROM, and
// starts out zeroed on every boot.
struct sdk_g_ic_volatile_st {
    void *_unknown0;
    void *_unknown4;

    uint8_t _unknown8[8];

    struct sdk_g_ic_netif_info *station_netif_info;
    struct sdk_g_ic_netif_info *softap_netif_info;
    uint8_t _unknown18;
    uint32_t _unknown1c;
    uint32_t _unknown20;

    uint8_t _unknown24[8];

    uint8_t _unknown2c;

    uint8_t _unknown30[76];

    uint8_t _unknown7c;
    uint8_t _unknown7d;
    uint8_t _unknown7e;
    uint8_t _unknown7f;

    uint32_t _unknown80;

    uint32_t _unknown84[50]; // wifi_softap_start, channels.

    void * volatile _unknown14c; // wifi_softap_start, current channel, arg to ieee80211_chan2ieee

    uint8_t _unknown150[20];

    uint32_t _unknown164;
    void *_unknown168;
    void *_unknown16c;
    void *_unknown170;
    void *_unknown174;
    void *_unknown178;

    uint8_t _unknown17c[4];

    void *_unknown180;
    void *_unknown184;
    struct station_info *station_info_head;
    struct station_info *station_info_tail;
    void *_unknown190[2]; // cnx_sta_leave

    uint8_t _unknown198[40];

    void *_unknown1c0;
    void *_unknown1c4;
    uint32_t _unknown1c8;

    uint8_t _unknown1cc[4];

    uint16_t _unknown1d0;

    uint8_t _unknown1d2[2];

    uint8_t _unknown1d4;

    uint8_t _unknown1d5[3];
};


struct sdk_g_ic_ssid_with_length {
    uint32_t ssid_length;  // 0x1e4 sdk_wpa_config_profile
    uint8_t ssid[32];  // 0x1e8 Station ssid. Null terminated string.
};

// This is the portion of g_ic which is loaded/saved to the flash ROM, and thus
// is preserved across reboots.
struct sdk_g_ic_saved_st {
    uint8_t _unknown1d8;
    uint8_t boot_info;
    uint8_t user0_addr[3];
    uint8_t user1_addr[3];
    uint8_t wifi_mode;
    uint8_t wifi_led_enable;
    uint8_t wifi_led_gpio;
    uint8_t wifi_led_state;  // 0 or 1.

    // Current station ap config ssid and length.
    struct sdk_g_ic_ssid_with_length sta_ssid; // 0x1e4

    uint8_t _unknown208;
    uint8_t _unknown209; // sdk_wpa_config_profile
    uint8_t _unknown20a; // sdk_wpa_config_profile
    uint8_t _unknown20b;
    uint8_t _unknown20c; // sdk_wpa_config_profile
    uint8_t _unknown20d;
    uint8_t _unknown20e;
    uint8_t sta_password[64]; // 0x20f Null terminated string.
    uint8_t _unknown24f;

    uint8_t _unknown250[49];

    uint8_t sta_bssid_set; // 0x281 One if bssid is used, otherwise zero.

    uint8_t sta_bssid[6]; // 0x282

    uint16_t _unknown288;
    uint16_t _unknown28a;
    uint8_t _unknown28c;

    uint8_t _unknown28d[21];

    uint8_t _unknown2a0; // used in dhcp_bind_check wpa_main.o

    uint8_t _unknown2a1[9];

    char _unknown2ac[64]; // string.
    uint8_t _unknonwn2ec;

    uint8_t _unknown2ed[32];

    uint8_t _unknown30d; // result of ieee80211_chan2ieee
    uint8_t _unknown30e;
    uint8_t _unknown30f;
    uint8_t _unknown310; // count of entries in the softap cnx_node array, less two.

    uint8_t _unknown311[3];

    uint8_t ap_number;
    uint8_t current_ap_id;

    uint8_t _unknown316[502];

    uint32_t _unknown50c;
    uint16_t _unknown510;
    uint16_t _unknown512;
    uint16_t _unknown514;

    uint8_t _unknown516[2];

    uint8_t auto_connect;

    uint8_t _unknown519[3];

    enum sdk_phy_mode phy_mode;

    uint8_t _unknown520[36];

    uint16_t _unknown544;

    uint8_t _unknown546[2];
};

struct sdk_g_ic_st {
    struct sdk_g_ic_volatile_st v; // 0x0 - 0x1d8
    struct sdk_g_ic_saved_st    s; // 0x1d8 - 0x548
};



struct esf_buf {
    struct pbuf *pbuf1;    // 0x00
    struct pbuf *pbuf2;    // 0x04
    uint32_t *_unknown8_;  // 0x08
    uint32_t *_unknownc_;  // 0x0c
    uint8_t *frame;        // 0x10 IEEE-802.11 payload data?
    uint16_t _unknown14_;  // 0x14
    uint16_t length;       // 0x16
    uint32_t *_unknown18_; // 0x18
    struct esf_buf *next;  // 0x1c Free list.
    void *extra;           // 0x20
};

///////////////////////////////////////////////////////////////////////////////
// The above structures all refer to data regions outside our control, and a
// simple mistake/misunderstanding editing things here can completely screw up
// how we access them, so do some basic sanity checks to make sure that they
// appear to match up correctly with the actual data structures other parts of
// the SDK are expecting.
///////////////////////////////////////////////////////////////////////////////

_Static_assert(sizeof(struct sdk_info_st) == 0x24, "info_st is the wrong size!");
_Static_assert(offsetof(struct sdk_info_st, sta_mac_addr) == 0x1e, "bad struct");

_Static_assert(offsetof(struct wl_channel, num) == 0x06, "bad struct");

_Static_assert(sizeof(struct _unknown_softap2) == 0xcc, "_unknown_softap2 is the wrong size!");
_Static_assert(offsetof(struct _unknown_softap2, _unknownb8) == 0xb8, "bad struct");

_Static_assert(sizeof(struct _unknown_softap1) == 0x1c, "_unknown_softap1 is the wrong size!");
_Static_assert(offsetof(struct _unknown_softap1, _unknown18) == 0x18, "bad struct");

_Static_assert(sizeof(struct _unknown_wpa1) == 0x4c, "_unknown_wpa1 is the wrong size!");
_Static_assert(offsetof(struct _unknown_wpa1, _unknown48) == 0x48, "bad struct");

_Static_assert(offsetof(struct sdk_cnx_node, channel) == 0x78, "bad struct");
_Static_assert(offsetof(struct sdk_cnx_node, _unknown108) == 0x108, "bad struct");

_Static_assert(offsetof(struct sdk_g_ic_netif_info, started) == 0xbb, "bad struct");

_Static_assert(sizeof(struct sdk_g_ic_volatile_st) == 0x1d8, "sdk_g_ic_volatile_st is the wrong size!");
_Static_assert(offsetof(struct sdk_g_ic_volatile_st, _unknown1d5) == 0x1d5, "bad struct");

_Static_assert(sizeof(struct sdk_g_ic_saved_st) == 0x370, "sdk_g_ic_saved_st is the wrong size!");
_Static_assert(offsetof(struct sdk_g_ic_saved_st, sta_ssid) == 0x1e4 - 0x1d8, "bad struct");
_Static_assert(offsetof(struct sdk_g_ic_saved_st, _unknown546) == 0x546 - 0x1d8, "bad struct");

_Static_assert(sizeof(struct sdk_g_ic_st) == 0x548, "sdk_g_ic_st is the wrong size!");

_Static_assert(sizeof(struct esf_buf) == 0x24, "struct esf_buf: wrong size");
_Static_assert(offsetof(struct esf_buf, extra) == 0x20, "bad struct");
_Static_assert(offsetof(struct esf_buf, length) == 0x16, "bad struct");

// The SDK access some slots in lwip structures.

// The netif->state is initialized in netif_add within lwip with a struct
// sdk_g_ic_netif_info, see sdk_wifi_station_start and sdk_wifi_softap_start.
// There is a known sdk read of the netif->state in ieee80211_output.o
// ieee80211_output_pbuf and perhaps elsewhere. The value is just passed through
// lwip and and not used by lwip so just ensure this slot is at the expected
// offset.
_Static_assert(offsetof(struct netif, state) == 4, "netif->state offset wrong!");

// Some sdk uses of netif->hwaddr have been converted to source code, but many
// remain, but the content of this slot should not change in future versions of
// lwip, so just ensure it is at the expected offset. Note the sdk binary
// libraries have been patched to move this offset from 41 to 42 to keep it
// 16-bit aligned to keep lwip v2 happy.
_Static_assert(offsetof(struct netif, hwaddr) == 8, "netif->hwaddr offset wrong!");

_Static_assert(offsetof(struct pbuf, esf_buf) == 16, "pbuf->esf_buf offset wrong!");


/// Misc.

err_t ethernetif_init(struct netif *netif);
void ethernetif_input(struct netif *netif, struct pbuf *p);

#endif /* _INTERNAL_SDK_STRUCTURES_H */
