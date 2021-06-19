/* Internal function declarations for Espressif SDK libpp functions.

   These are internal-facing declarations, it is not recommended to include these headers in your program.
   (look at the headers in include/espressif/ instead and use these whenever possible.)

   Copyright (C) 2015 Espressif Systems. Derived from MIT Licensed SDK libraries.
   BSD Licensed as described in the file LICENSE.
*/
#ifndef _ESPLIBS_LIBPP_H
#define _ESPLIBS_LIBPP_H

#include "sdk_internal.h"

// esf_buf.o
struct esf_buf *sdk_esf_rx_buf_alloc(uint32_t n); // n must be 7.
struct esf_buf *sdk_esf_buf_alloc(void *, uint32_t n);
void sdk_esf_buf_recycle(struct esf_buf *buf, int);
void sdk_esf_buf_setup(void);

// if_hwctrl.o
extern uint8_t sdk_interface_mask;
//extern ? sdk_if_ctrl;
void sdk_ic_set_vif(int, int, uint8_t (*)[6], int, int);
void sdk_ic_bss_info_update(int, uint8_t (*hwaddr)[], int, int);
void sdk_ic_set_sta(int, int, void *, int, int, int, int, int);
void sdk_ic_remove_key(uint32_t);

// lmac.o
extern uint32_t sdk_lmacConfMib;
void sdk_lmacInit(void);

// mac_frame.o

// pm.o
struct esf_buf *sdk_ieee80211_getmgtframe(void **arg0, uint32_t arg1, uint32_t arg2);
void sdk_pm_attach(void);
uint32_t sdk_pm_rtc_clock_cali_proc(void);
void sdk_pm_set_sleep_time(uint32_t);
uint8_t sdk_pm_is_waked(void);
bool sdk_pm_is_open(void);
bool sdk_pm_post(int);
enum sdk_sleep_type sdk_pm_get_sleep_type(void);
void sdk_pm_set_sleep_type_from_upper(enum sdk_sleep_type);


// pp.o
extern uint16_t sdk_NoiseTimerInterval;
extern uint16_t sdk_sleep_start_wait_time;
extern uint8_t sdk_pend_flag_noise_check;
extern uint8_t sdk_pend_flag_periodic_cal;
extern uint8_t sdk_dbg_stop_sw_wdt;
extern uint8_t sdk_dbg_stop_hw_wdt;
bool sdk_ppRegisterTxCallback(void *, int);
bool sdk_ppTxPkt(struct esf_buf *);
void sdk_ppRecycleRxPkt(void *);
void sdk_pp_attach(void);
void sdk_pp_soft_wdt_init(void);
void sdk_pp_soft_wdt_feed();
void sdk_pp_post(int, int);

// rate_control.o

// trc.o

// wdev.o
extern uint8_t sdk_NMIIrqIsOn;
extern uint32_t sdk_WdevTimOffSet;
uint32_t sdk_wDev_Get_Next_TBTT();
void sdk_wDev_Reset_TBTT();
void sdk_wDev_SetRxPolicy(int, int, int);
void sdk_wDevEnableRx(void);
void sdk_wDev_Initialize(void);
void sdk_wDev_MacTim1SetFunc(void (*func)(void));
void sdk_wDev_MacTim1Arm(uint32_t);

#endif /* _ESPLIBS_LIBPP_H */

