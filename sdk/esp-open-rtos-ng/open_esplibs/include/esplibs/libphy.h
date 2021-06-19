/* Internal function declarations for Espressif SDK libphy functions.

   These are internal-facing declarations, it is not recommended to include these headers in your program.
   (look at the headers in include/espressif/ instead and use these whenever possible.)

   Copyright (C) 2015 Espressif Systems. Derived from MIT Licensed SDK libraries.
   BSD Licensed as described in the file LICENSE.
*/
#ifndef _ESPLIBS_LIBPHY_H
#define _ESPLIBS_LIBPHY_H

#include "sdk_internal.h"

// phy_chip_v5_ana_romfunc.o

// phy_chip_v5_cal_romfunc.o

// phy_chip_v5_romfunc.o

// phy_chip_v6_ana.o
uint32_t sdk_test_tout(bool);
uint32_t sdk_readvdd33();

// phy_chip_v6_cal.o
extern uint16_t sdk_loop_pwctrl_pwdet_error_accum_high_power;
extern uint8_t sdk_tx_pwctrl_pk_num;
extern uint8_t sdk_loop_pwctrl_correct_atten_high_power;
extern uint8_t sdk_tx_pwctrl_set_chan_flag;
extern uint8_t sdk_rxiq_cover_fail_num;

// phy_chip_v6.o
extern uint16_t sdk_tx_rf_ana_gain;
extern uint32_t sdk_rxiq_compute_num;
extern uint8_t sdk_rxdc_init_flag;
extern uint32_t sdk_check_result;
extern uint32_t sdk_chip6_sleep_params;
extern uint8_t sdk_chip6_phy_init_ctrl;
extern uint32_t sdk_phy_freq_offset;
extern uint8_t sdk_do_pwctrl_flag;
extern uint8_t sdk_pwctrl_debug;
extern uint8_t sdk_txbk_dpdby_flag;
extern uint8_t sdk_sw_scan_mode;
extern uint32_t sdk_periodic_cal_dc_num;
extern uint8_t sdk_periodic_cal_flag;
extern uint8_t sdk_bbpll_cal_flag;
extern uint8_t sdk_deep_sleep_en;
int sdk_register_chipv6_phy(sdk_phy_info_t *);

// phy_chip_v6_unused.o

// phy.o
void sdk_phy_disable_agc(void);
void sdk_phy_enable_agc(void);

// phy_sleep.o
extern uint32_t sdk_chip_version;
extern uint8_t sdk_periodic_cal_sat;
extern uint8_t sdk_software_slp_reject;
extern uint8_t sdk_SDIO_slp_reject;
extern uint8_t sdk_hardware_reject;
void sdk_sleep_reset_analog_rtcreg_8266(void);


#endif /* _ESPLIBS_LIBPHY_H */

