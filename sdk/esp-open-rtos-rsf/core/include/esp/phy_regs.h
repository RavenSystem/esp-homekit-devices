/* esp/phy_regs.h
 *
 * ESP8266 PHY register definitions
 *
 * Not compatible with ESP SDK register access code.
 */

#ifndef _ESP_PHY_REGS_H
#define _ESP_PHY_REGS_H

#include "esp/types.h"
#include "common_macros.h"

#define PHY_BASE 0x60000500
#define PHY (*(struct PHY_REGS *)(PHY_BASE))

struct PHY_REGS {
    // 0x00 — 0x60
    uint32_t volatile _gap0[24];
    // TX digital predistortion control
    // 0x60
    uint32_t volatile TX_DPD;
    // 0x64 — 0x7c
    uint32_t volatile _gap1[6];
    // IQ imbalance estimation control
    // 0x7c
    uint32_t volatile IQ_EST;
    // Looks like RSSI,
    // may be per OFDM subcarrier
    // 0x80
    uint32_t volatile RX_IQ_0;
    // 0x84
    uint32_t volatile RX_IQ_1;
    // 0x88
    uint32_t volatile RX_IQ_2;
    // 0x8c
    uint32_t volatile RX_IQ_3;
    // RX gain control
    // 0x90
    uint32_t volatile RX_GAIN_CTL;
    // Whatever pbus is, these registers controls it
    // 0x94
    uint32_t volatile PBUS_CTL_0;
    // 0x98
    uint32_t volatile PBUS_CTL_1;
    // 0x9C
    uint32_t volatile PBUS_CTL_2;
    // 0xA0
    uint32_t volatile PBUS_CTL_3;
    uint32_t volatile _gap2[5];
    // Looks like baseband synthesizer control regs
    // 0xB8
    uint32_t volatile BB_CTL_0;
    // 0xBC
    uint32_t volatile BB_CTL_1;
    // 0xC0
    uint32_t volatile BB_CTL_2;
    // 0xC4
    uint32_t volatile BB_CTL_3;
    // These registers do exist but I don't know
    // what they are for.
    // 0xC8
    uint32_t volatile UNK_0;
    uint32_t volatile UNK_1;
    uint32_t volatile UNK_2;
    uint32_t volatile UNK_3;
    uint32_t volatile UNK_4;
};

_Static_assert((uintptr_t) &PHY.TX_DPD == 0x60000560, "RF PHY TX_DPD address");
_Static_assert((uintptr_t) &PHY.IQ_EST == 0x6000057C, "RF PHY IQ_EST address");
_Static_assert((uintptr_t) &PHY.BB_CTL_3 == 0x600005C4, "RF PHY BB_CTL_3 address");

#endif /* _ESP_PHY_REGS_H */
