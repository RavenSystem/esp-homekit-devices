/* esp/i2s_regs.h
 *
 * ESP8266 I2S register definitions
 *
 * Not compatible with ESP SDK register access code.
 */

#ifndef _ESP_I2S_REGS_H
#define _ESP_I2S_REGS_H

#include "esp/types.h"
#include "common_macros.h"

#define I2S_BASE 0x60000e00
#define I2S (*(struct I2S_REGS *)I2S_BASE)

struct I2S_REGS {
    uint32_t volatile TXFIFO;           // 0x00
    uint32_t volatile RXFIFO;           // 0x04
    uint32_t volatile CONF;             // 0x08
    uint32_t volatile INT_RAW;          // 0x0c
    uint32_t volatile INT_STATUS;       // 0x10
    uint32_t volatile INT_ENABLE;       // 0x14
    uint32_t volatile INT_CLEAR;        // 0x18
    uint32_t volatile TIMING;           // 0x1c
    uint32_t volatile FIFO_CONF;        // 0x20
    uint32_t volatile RX_EOF_NUM;       // 0x24
    uint32_t volatile CONF_SINGLE_DATA; // 0x28
    uint32_t volatile CONF_CHANNELS;    // 0x2c
};

_Static_assert(sizeof(struct I2S_REGS) == 0x30, "I2S_REGS is the wrong size");

/* Details for CONF register */

#define I2S_CONF_BCK_DIV_M      0x0000003f
#define I2S_CONF_BCK_DIV_S      22
#define I2S_CONF_CLKM_DIV_M     0x0000003f
#define I2S_CONF_CLKM_DIV_S     16
#define I2S_CONF_BITS_MOD_M     0x0000000f
#define I2S_CONF_BITS_MOD_S     12
#define I2S_CONF_RX_MSB_SHIFT   BIT(11)
#define I2S_CONF_TX_MSB_SHIFT   BIT(10)
#define I2S_CONF_RX_START       BIT(9)
#define I2S_CONF_TX_START       BIT(8)
#define I2S_CONF_MSB_RIGHT      BIT(7)
#define I2S_CONF_RIGHT_FIRST    BIT(6)
#define I2S_CONF_RX_SLAVE_MOD   BIT(5)
#define I2S_CONF_TX_SLAVE_MOD   BIT(4)
#define I2S_CONF_RX_FIFO_RESET  BIT(3)
#define I2S_CONF_TX_FIFO_RESET  BIT(2)
#define I2S_CONF_RX_RESET       BIT(1)
#define I2S_CONF_TX_RESET       BIT(0)
#define I2S_CONF_RESET_MASK     0xf

/* Details for INT_RAW register */

#define I2S_INT_RAW_TX_REMPTY    BIT(5)
#define I2S_INT_RAW_TX_WFULL     BIT(4)
#define I2S_INT_RAW_RX_REMPTY    BIT(3)
#define I2S_INT_RAW_RX_WFULL     BIT(2)
#define I2S_INT_RAW_TX_PUT_DATA  BIT(1)
#define I2S_INT_RAW_RX_TAKE_DATA BIT(0)

/* Details for INT_STATUS register */

#define I2S_INT_STATUS_TX_REMPTY     BIT(5)
#define I2S_INT_STATUS_TX_WFULL      BIT(4)
#define I2S_INT_STATUS_RX_REMPTY     BIT(3)
#define I2S_INT_STATUS_RX_WFULL      BIT(2)
#define I2S_INT_STATUS_TX_PUT_DATA   BIT(1)
#define I2S_INT_STATUS_RX_TAKE_DATA  BIT(0)

/* Details for INT_ENABLE register */

#define I2S_INT_ENABLE_TX_REMPTY     BIT(5)
#define I2S_INT_ENABLE_TX_WFULL      BIT(4)
#define I2S_INT_ENABLE_RX_REMPTY     BIT(3)
#define I2S_INT_ENABLE_RX_WFULL      BIT(2)
#define I2S_INT_ENABLE_TX_PUT_DATA   BIT(1)
#define I2S_INT_ENABLE_RX_TAKE_DATA  BIT(0)

/* Details for INT_CLEAR register */

#define I2S_INT_CLEAR_TX_REMPTY     BIT(5)
#define I2S_INT_CLEAR_TX_WFULL      BIT(4)
#define I2S_INT_CLEAR_RX_REMPTY     BIT(3)
#define I2S_INT_CLEAR_RX_WFULL      BIT(2)
#define I2S_INT_CLEAR_TX_PUT_DATA   BIT(1)
#define I2S_INT_CLEAR_RX_TAKE_DATA  BIT(0)

/* Details for TIMING register */

#define I2S_TIMING_TX_BCK_IN_INV       BIT(22)
#define I2S_TIMING_RX_DSYNC_SW         BIT(21)
#define I2S_TIMING_TX_DSYNC_SW         BIT(20)
#define I2S_TIMING_RX_BCK_OUT_DELAY_M  0x00000003
#define I2S_TIMING_RX_BCK_OUT_DELAY_S  18
#define I2S_TIMING_RX_WS_OUT_DELAY_M   0x00000003
#define I2S_TIMING_RX_WS_OUT_DELAY_S   16
#define I2S_TIMING_TX_SD_OUT_DELAY_M   0x00000003
#define I2S_TIMING_TX_SD_OUT_DELAY_S   14
#define I2S_TIMING_TX_WS_OUT_DELAY_M   0x00000003
#define I2S_TIMING_TX_WS_OUT_DELAY_S   12
#define I2S_TIMING_TX_BCK_OUT_DELAY_M  0x00000003
#define I2S_TIMING_TX_BCK_OUT_DELAY_S  10
#define I2S_TIMING_RX_SD_IN_DELAY_M    0x00000003
#define I2S_TIMING_RX_SD_IN_DELAY_S    8
#define I2S_TIMING_RX_WS_IN_DELAY      0x00000003
#define I2S_TIMING_RX_WS_IN_DELAY_S    6
#define I2S_TIMING_RX_BCK_IN_DELAY_M   0x00000003
#define I2S_TIMING_RX_BCK_IN_DELAY_S   4
#define I2S_TIMING_TX_WS_IN_DELAY_M    0x00000003
#define I2S_TIMING_TX_WS_IN_DELAY_S    2
#define I2S_TIMING_TX_BCK_IN_DELAY_M   0x00000003
#define I2S_TIMING_TX_BCK_IN_DELAY_S   0

/* Details for FIFO_CONF register */

#define I2S_FIFO_CONF_RX_FIFO_MOD_M      0x00000007
#define I2S_FIFO_CONF_RX_FIFO_MOD_S      16
#define I2S_FIFO_CONF_TX_FIFO_MOD_M      0x00000007
#define I2S_FIFO_CONF_TX_FIFO_MOD_S      13
#define I2S_FIFO_CONF_DESCRIPTOR_ENABLE  BIT(12)
#define I2S_FIFO_CONF_TX_DATA_NUM_M      0x0000003f
#define I2S_FIFO_CONF_TX_DATA_NUM_S      6
#define I2S_FIFO_CONF_RX_DATA_NUM_M      0x0000003f
#define I2S_FIFO_CONF_RX_DATA_NUM_S      0

/* Details for CONF_CHANNEL register */

#define I2S_CONF_CHANNELS_RX_CHANNEL_MOD_M  0x00000003
#define I2S_CONF_CHANNELS_RX_CHANNEL_MOD_S  3
#define I2S_CONF_CHANNELS_TX_CHANNEL_MOD_M  0x00000007
#define I2S_CONF_CHANNELS_TX_CHANNEL_MOD_S  0

#endif /* _ESP_I2S_REGS_H */
