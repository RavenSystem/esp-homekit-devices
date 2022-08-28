/* esp/slc_regs.h
 *
 * ESP8266 SLC register definitions
 *
 * Not compatible with ESP SDK register access code.
 */

#ifndef _ESP_SLC_REGS_H
#define _ESP_SLC_REGS_H

#include "esp/types.h"
#include "common_macros.h"

#define SLC_BASE 0x60000b00
#define SLC (*(struct SLC_REGS *)SLC_BASE)

struct SLC_REGS {
    uint32_t volatile CONF0;                   // 0x00
    uint32_t volatile INT_RAW;                 // 0x04
    uint32_t volatile INT_STATUS;              // 0x08
    uint32_t volatile INT_ENABLE;              // 0x0c
    uint32_t volatile INT_CLEAR;               // 0x10
    uint32_t volatile RX_STATUS;               // 0x14
    uint32_t volatile RX_FIFO_PUSH;            // 0x18
    uint32_t volatile TX_STATUS;               // 0x1c
    uint32_t volatile TX_FIFO_POP;             // 0x20
    uint32_t volatile RX_LINK;                 // 0x24
    uint32_t volatile TX_LINK;                 // 0x28
    uint32_t volatile INTVEC_TO_HOST;          // 0x2c
    uint32_t volatile TOKEN0;                  // 0x30
    uint32_t volatile TOKEN1;                  // 0x34
    uint32_t volatile CONF1;                   // 0x38
    uint32_t volatile STATE0;                  // 0x3c
    uint32_t volatile STATE1;                  // 0x40
    uint32_t volatile BRIDGE_CONF;             // 0x44
    uint32_t volatile RX_EOF_DESCRIPTOR_ADDR;  // 0x48
    uint32_t volatile TX_EOF_DESCRIPTOR_ADDR;  // 0x4c
    uint32_t volatile RX_EOF_BUFFER_DESCRIPTOR_ADDR; // 0x50 - Naming uncertain
    uint32_t volatile AHB_TEST;                // 0x54
    uint32_t volatile SDIO_STATUS;             // 0x58
    uint32_t volatile RX_DESCRIPTOR_CONF;      // 0x5c
    uint32_t volatile TX_LINK_DESCRIPTOR;      // 0x60
    uint32_t volatile TX_LINK_DESCRIPTOR_BF0;  // 0x64
    uint32_t volatile TX_LINK_DESCRIPTOR_BF1;  // 0x68
    uint32_t volatile RX_LINK_DESCRIPTOR;      // 0x6c
    uint32_t volatile RX_LINK_DESCRIPTOR_BF0;  // 0x70
    uint32_t volatile RX_LINK_DESCRIPTOR_BF1;  // 0x74
    uint32_t volatile DATE;                    // 0x78
    uint32_t volatile ID;                      // 0x7c
    uint32_t volatile UNKNOWN_80;              // 0x80
    uint32_t volatile UNKNOWN_84;              // 0x84
    uint32_t volatile HOST_INT_RAW;            // 0x88
    uint32_t volatile UNKNOWN_8C;              // 0x8c
    uint32_t volatile UNKNOWN_90;              // 0x90
    uint32_t volatile HOST_CONF_W0;            // 0x94
    uint32_t volatile HOST_CONF_W1;            // 0x98
    uint32_t volatile HOST_INT_STATUS;         // 0x9c
    uint32_t volatile HOST_CONF_W2;            // 0xa0
    uint32_t volatile HOST_CONF_W3;            // 0xa4
    uint32_t volatile HOST_CONF_W4;            // 0xa8
    uint32_t volatile UNKNOWN_AC;              // 0xac
    uint32_t volatile HOST_INT_CLEAR;          // 0xb0
    uint32_t volatile HOST_INT_ENABLE;         // 0xb4
    uint32_t volatile UNKNOWN_B8;              // 0xb8
    uint32_t volatile HOST_CONF_W5;            // 0xbc
};

_Static_assert(sizeof(struct SLC_REGS) == 0xc0, "SLC_REGS is the wrong size");

/* Details for CONF0 register */

#define SLC_CONF0_MODE_M                   0x00000003
#define SLC_CONF0_MODE_S                   12
#define SLC_CONF0_DATA_BURST_ENABLE        BIT(9)
#define SLC_CONF0_DESCRIPTOR_BURST_ENABLE  BIT(8)
#define SLC_CONF0_RX_NO_RESTART_CLEAR      BIT(7)
#define SLC_CONF0_RX_AUTO_WRITE_BACK       BIT(6)
#define SLC_CONF0_RX_LOOP_TEST             BIT(5)
#define SLC_CONF0_TX_LOOP_TEST             BIT(4)
#define SLC_CONF0_AHBM_RESET               BIT(3)
#define SLC_CONF0_AHBM_FIFO_RESET          BIT(2)
#define SLC_CONF0_RX_LINK_RESET            BIT(1)
#define SLC_CONF0_TX_LINK_RESET            BIT(0)

/* Details for INT_RAW register */

#define SLC_INT_RAW_TX_DSCR_EMPTY   BIT(21)
#define SLC_INT_RAW_RX_DSCR_ERROR   BIT(20)
#define SLC_INT_RAW_TX_DSCR_ERROR   BIT(19)
#define SLC_INT_RAW_TO_HOST         BIT(18)
#define SLC_INT_RAW_RX_EOF          BIT(17)
#define SLC_INT_RAW_RX_DONE         BIT(16)
#define SLC_INT_RAW_TX_EOF          BIT(15)
#define SLC_INT_RAW_TX_DONE         BIT(14)
#define SLC_INT_RAW_TOKEN1_1TO0     BIT(13)
#define SLC_INT_RAW_TOKEN0_1TO0     BIT(12)
#define SLC_INT_RAW_TX_OVERFLOW     BIT(11)
#define SLC_INT_RAW_RX_UNDEFLOW     BIT(10)
#define SLC_INT_RAW_TX_START        BIT(9)
#define SLC_INT_RAW_RX_START        BIT(8)
#define SLC_INT_RAW_FROM_HOST_BIT7  BIT(7)
#define SLC_INT_RAW_FROM_HOST_BIT6  BIT(6)
#define SLC_INT_RAW_FROM_HOST_BIT5  BIT(5)
#define SLC_INT_RAW_FROM_HOST_BIT4  BIT(4)
#define SLC_INT_RAW_FROM_HOST_BIT3  BIT(3)
#define SLC_INT_RAW_FROM_HOST_BIT2  BIT(2)
#define SLC_INT_RAW_FROM_HOST_BIT1  BIT(1)
#define SLC_INT_RAW_FROM_HOST_BIT0  BIT(0)

/* Details for INT_STATUS register */

#define SLC_INT_STATUS_TX_DSCR_EMPTY   BIT(21)
#define SLC_INT_STATUS_RX_DSCR_ERROR   BIT(20)
#define SLC_INT_STATUS_TX_DSCR_ERROR   BIT(19)
#define SLC_INT_STATUS_TO_HOST         BIT(18)
#define SLC_INT_STATUS_RX_EOF          BIT(17)
#define SLC_INT_STATUS_RX_DONE         BIT(16)
#define SLC_INT_STATUS_TX_EOF          BIT(15)
#define SLC_INT_STATUS_TX_DONE         BIT(14)
#define SLC_INT_STATUS_TOKEN1_1TO0     BIT(13)
#define SLC_INT_STATUS_TOKEN0_1TO0     BIT(12)
#define SLC_INT_STATUS_TX_OVERFLOW     BIT(11)
#define SLC_INT_STATUS_RX_UNDEFLOW     BIT(10)
#define SLC_INT_STATUS_TX_START        BIT(9)
#define SLC_INT_STATUS_RX_START        BIT(8)
#define SLC_INT_STATUS_FROM_HOST_BIT7  BIT(7)
#define SLC_INT_STATUS_FROM_HOST_BIT6  BIT(6)
#define SLC_INT_STATUS_FROM_HOST_BIT5  BIT(5)
#define SLC_INT_STATUS_FROM_HOST_BIT4  BIT(4)
#define SLC_INT_STATUS_FROM_HOST_BIT3  BIT(3)
#define SLC_INT_STATUS_FROM_HOST_BIT2  BIT(2)
#define SLC_INT_STATUS_FROM_HOST_BIT1  BIT(1)
#define SLC_INT_STATUS_FROM_HOST_BIT0  BIT(0)

/* Details for INT_ENABLE register */

#define SLC_INT_ENABLE_TX_DSCR_EMPTY   BIT(21)
#define SLC_INT_ENABLE_RX_DSCR_ERROR   BIT(20)
#define SLC_INT_ENABLE_TX_DSCR_ERROR   BIT(19)
#define SLC_INT_ENABLE_TO_HOST         BIT(18)
#define SLC_INT_ENABLE_RX_EOF          BIT(17)
#define SLC_INT_ENABLE_RX_DONE         BIT(16)
#define SLC_INT_ENABLE_TX_EOF          BIT(15)
#define SLC_INT_ENABLE_TX_DONE         BIT(14)
#define SLC_INT_ENABLE_TOKEN1_1TO0     BIT(13)
#define SLC_INT_ENABLE_TOKEN0_1TO0     BIT(12)
#define SLC_INT_ENABLE_TX_OVERFLOW     BIT(11)
#define SLC_INT_ENABLE_RX_UNDEFLOW     BIT(10)
#define SLC_INT_ENABLE_TX_START        BIT(9)
#define SLC_INT_ENABLE_RX_START        BIT(8)
#define SLC_INT_ENABLE_FROM_HOST_BIT7  BIT(7)
#define SLC_INT_ENABLE_FROM_HOST_BIT6  BIT(6)
#define SLC_INT_ENABLE_FROM_HOST_BIT5  BIT(5)
#define SLC_INT_ENABLE_FROM_HOST_BIT4  BIT(4)
#define SLC_INT_ENABLE_FROM_HOST_BIT3  BIT(3)
#define SLC_INT_ENABLE_FROM_HOST_BIT2  BIT(2)
#define SLC_INT_ENABLE_FROM_HOST_BIT1  BIT(1)
#define SLC_INT_ENABLE_FROM_HOST_BIT0  BIT(0)

#define SLC_INT_ENABLE_FROM_HOST_BIT_ALL  0xff

/* Details for INT_CLEAR register */

#define SLC_INT_CLEAR_TX_DSCR_EMPTY   BIT(21)
#define SLC_INT_CLEAR_RX_DSCR_ERROR   BIT(20)
#define SLC_INT_CLEAR_TX_DSCR_ERROR   BIT(19)
#define SLC_INT_CLEAR_TO_HOST         BIT(18)
#define SLC_INT_CLEAR_RX_EOF          BIT(17)
#define SLC_INT_CLEAR_RX_DONE         BIT(16)
#define SLC_INT_CLEAR_TX_EOF          BIT(15)
#define SLC_INT_CLEAR_TX_DONE         BIT(14)
#define SLC_INT_CLEAR_TOKEN1_1TO0     BIT(13)
#define SLC_INT_CLEAR_TOKEN0_1TO0     BIT(12)
#define SLC_INT_CLEAR_TX_OVERFLOW     BIT(11)
#define SLC_INT_CLEAR_RX_UNDEFLOW     BIT(10)
#define SLC_INT_CLEAR_TX_START        BIT(9)
#define SLC_INT_CLEAR_RX_START        BIT(8)
#define SLC_INT_CLEAR_FROM_HOST_BIT7  BIT(7)
#define SLC_INT_CLEAR_FROM_HOST_BIT6  BIT(6)
#define SLC_INT_CLEAR_FROM_HOST_BIT5  BIT(5)
#define SLC_INT_CLEAR_FROM_HOST_BIT4  BIT(4)
#define SLC_INT_CLEAR_FROM_HOST_BIT3  BIT(3)
#define SLC_INT_CLEAR_FROM_HOST_BIT2  BIT(2)
#define SLC_INT_CLEAR_FROM_HOST_BIT1  BIT(1)
#define SLC_INT_CLEAR_FROM_HOST_BIT0  BIT(0)

/* Details for RX_STATUS register */

#define SLC_RX_STATUS_EMPTY  BIT(1)
#define SLC_RX_STATUS_FULL   BIT(0)

/* Details for RX_FIFO_PUSH register */

#define SLC_RX_FIFO_PUSH_FLAG    BIT(16)
#define SLC_RX_FIFO_PUSH_DATA_M  0x000001ff
#define SLC_RX_FIFO_PUSH_DATA_S  0

/* Details for TX_STATUS register */

#define SLC_TX_STATUS_EMPTY  BIT(1)
#define SLC_TX_STATUS_FULL   BIT(0)

/* Details for TX_FIFO_POP register */

#define SLC_TX_FIFO_POP_FLAG    BIT(16)
#define SLC_TX_FIFO_POP_DATA_M  0x000007ff
#define SLC_TX_FIFO_POP_DATA_S  0

/* Details for RX_LINK register */

#define SLC_RX_LINK_PARK               BIT(31)
#define SLC_RX_LINK_RESTART            BIT(30)
#define SLC_RX_LINK_START              BIT(29)
#define SLC_RX_LINK_STOP               BIT(28)
#define SLC_RX_LINK_DESCRIPTOR_ADDR_M  0x000fffff
#define SLC_RX_LINK_DESCRIPTOR_ADDR_S  0

/* Details for TX_LINK register */

#define SLC_TX_LINK_PARK               BIT(31)
#define SLC_TX_LINK_RESTART            BIT(30)
#define SLC_TX_LINK_START              BIT(29)
#define SLC_TX_LINK_STOP               BIT(28)
#define SLC_TX_LINK_DESCRIPTOR_ADDR_M  0x000fffff
#define SLC_TX_LINK_DESCRIPTOR_ADDR_S  0

/* Details for INTVEC_TO_HOST register */

#define SLC_INTVEC_TO_HOST_INTVEC_M 0x000000ff
#define SLC_INTVEC_TO_HOST_INTVEC_S 0

/* Details for TOKEN0 register */

#define SLC_TOKEN0_M               0x00000fff
#define SLC_TOKEN0_S               16
#define SLC_TOKEN0_LOCAL_INC_MORE  BIT(14)
#define SLC_TOKEN0_LOCAL_INC       BIT(13)
#define SLC_TOKEN0_LOCAL_WRITE     BIT(12)
#define SLC_TOKEN0_LOCAL_DATA_M    0x00000FFF
#define SLC_TOKEN0_LOCAL_DATA_S    0

/* Details for TOKEN1 register */

#define SLC_TOKEN1_MASK            0x00000fff
#define SLC_TOKEN1_S               16
#define SLC_TOKEN1_LOCAL_INC_MORE  BIT(14)
#define SLC_TOKEN1_LOCAL_INC       BIT(13)
#define SLC_TOKEN1_LOCAL_WRITE     BIT(12)
#define SLC_TOKEN1_LOCAL_DATA_M    0x00000fff
#define SLC_TOKEN1_LOCAL_DATA_S    0

/* Details for BRIDGE_CONF register */

#define SLC_BRIDGE_CONF_TX_PUSH_IDLE_M     0x0000ffff
#define SLC_BRIDGE_CONF_TX_PUSH_IDLE_S     16
#define SLC_BRIDGE_CONF_TX_DUMMY_MODE      BIT(12)
#define SLC_BRIDGE_CONF_FIFO_MAP_ENABLE_M  0x0000000f
#define SLC_BRIDGE_CONF_FIFO_MAP_ENABLE_S  8
#define SLC_BRIDGE_CONF_TX_EOF_ENABLE_M    0x0000003f
#define SLC_BRIDGE_CONF_TX_EOF_ENABLE_S    0

/* Details for AHB_TEST register */

#define SLC_AHB_TEST_ADDR_M 0x00000003
#define SLC_AHB_TEST_ADDR_S 4
#define SLC_AHB_TEST_MODE_M 0x00000007
#define SLC_AHB_TEST_MODE_S 0

/* Details for SDIO_STATUS register */

#define SLC_SDIO_STATUS_BUS_M      0x00000007
#define SLC_SDIO_STATUS_BUS_S      12
#define SLC_SDIO_STATUS_WAKEUP     BIT(8)
#define SLC_SDIO_STATUS_FUNC_M     0x0000000f
#define SLC_SDIO_STATUS_FUNC_S     4
#define SLC_SDIO_STATUS_COMMAND_M  0x00000007
#define SLC_SDIO_STATUS_COMMAND_S  0

/* Details for RX_DESCRIPTOR_CONF register */

#define SLC_RX_DESCRIPTOR_CONF_RX_FILL_ENABLE    BIT(20)
#define SLC_RX_DESCRIPTOR_CONF_RX_EOF_MODE       BIT(19)
#define SLC_RX_DESCRIPTOR_CONF_RX_FILL_MODE      BIT(18)
#define SLC_RX_DESCRIPTOR_CONF_INFOR_NO_REPLACE  BIT(17)
#define SLC_RX_DESCRIPTOR_CONF_TOKEN_NO_REPLACE  BIT(16)
#define SLC_RX_DESCRIPTOR_CONF_POP_IDLE_COUNT_M  0x0000ffff
#define SLC_RX_DESCRIPTOR_CONF_POP_IDLE_COUNT_S  0

#endif /* _ESP_SLC_REGS_H */
