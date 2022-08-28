/* Internal function declarations for Espressif SDK libpp functions.

   These are internal-facing declarations, it is not recommended to include these headers in your program.
   (look at the headers in include/espressif/ instead and use these whenever possible.)

   Copyright (C) 2015 Espressif Systems. Derived from MIT Licensed SDK libraries.
   BSD Licensed as described in the file LICENSE.
*/
#ifndef _ESPLIBS_LIBWPA_H
#define _ESPLIBS_LIBWPA_H

#include "sdk_internal.h"

// aes-internal-dec.o

// aes-internal-enc.o

// aes-internal.o

// aes-unwrap.o

// aes-wrap.o

// ap_config.o

// Seems to be passed 3 args, but only uses 2?
int sdk_hostapd_setup_wpa_psk(struct _unknown_softap2 *);

// common.o

// ieee802_1x.o

// md5-internal.o

// md5.o

// os_xtensa.o
int sdk_os_get_time(uint32_t time[]);
uint32_t sdk_os_random();
int sdk_os_get_random(uint8_t *dst, uint32_t size);

// rc4.o

// sha1-internal.o

// sha1.o

// sha1-pbkdf2.o

// sta_info.o

// wpa_auth_ie.o

// wpa_auth.o
uint32_t *sdk_wpa_init(uint8_t (*hwaddr)[], struct _unknown_wpa1 *, int);
void sdk_wpa_auth_sta_deinit(void *);

// wpabuf.o

// wpa_common.o

// wpa_debug.o

// wpa_ie.o

// wpa_main
void sdk_ppInstallKey(void *, int, int);
void sdk_wpa_config_profile(struct sdk_g_ic_st *);
void sdk_wpa_config_bss(struct sdk_g_ic_st *g_ic, uint8_t (* hwaddr2)[6]);
void sdk_wpa_config_assoc_ie(int , int16_t *, int32_t);
void sdk_dhcp_bind_check();
void sdk_eagle_auth_done();
void sdk_wpa_neg_complete();
void sdk_wpa_attach(struct sdk_g_ic_st *);

// wpa.o
void sdk_wpa_set_profile(uint8_t);
void sdk_wpa_set_bss(uint8_t *hwaddr1, uint8_t (* hwaddr2)[6], uint8_t, uint8_t, uint8_t *, uint8_t *ssid, int);
void sdk_eapol_txcb();
void sdk_wpa_register(int, void *, void *, void *, void *, void *);

// wpas_glue.o

#endif /* _ESPLIBS_LIBWPA_H */
