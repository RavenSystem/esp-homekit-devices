/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 sheinz (https://github.com/sheinz)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "include/spiflash.h"

#include "include/flashchip.h"
#include "include/esp/rom.h"
#include "include/esp/spi_regs.h"

#include <FreeRTOS.h>
#include <string.h>

/**
 * Note about Wait_SPI_Idle.
 *
 * Each write/erase flash operation sets BUSY bit in flash status register.
 * If attempt to access flash while BUSY bit is set operation will fail.
 * Function Wait_SPI_Idle loops until this bit is not cleared.
 *
 * The approach in the following code is that each write function that is
 * accessible from the outside should leave flash in Idle state.
 * The read operations doesn't set BUSY bit in a flash. So they do not wait.
 * They relay that previous operation is completely finished.
 *
 * This approach is different from ESP8266 bootrom where Wait_SPI_Idle is
 * called where it needed and not.
 */

#define SPI_WRITE_MAX_SIZE  64

// 64 bytes read causes hang
// http://bbs.espressif.com/viewtopic.php?f=6&t=2439
#define SPI_READ_MAX_SIZE   60



#define SPI_FLASH_ISSI_ENABLE_QIO_MODE          (BIT(6))

/*gd25q32c*/
#define SPI_FLASH_GD25Q32C_WRITE_STATUSE1_CMD   (0X01)
#define SPI_FLASH_GD25Q32C_WRITE_STATUSE2_CMD   (0X31)
#define SPI_FLASH_GD25Q32C_WRITE_STATUSE3_CMD   (0X11)

#define SPI_FLASH_GD25Q32C_READ_STATUSE1_CMD    (0X05)
#define SPI_FLASH_GD25Q32C_READ_STATUSE2_CMD    (0X35)
#define SPI_FLASH_GD25Q32C_READ_STATUSE3_CMD    (0X15)

#define SPI_FLASH_GD25Q32C_QIO_MODE             (BIT(1))

#define SPI_ISSI_FLASH_WRITE_PROTECT_STATUS     (BIT(2)|BIT(3)|BIT(4)|BIT(5))
#define SPI_EON_25Q16A_WRITE_PROTECT_STATUS     (BIT(2)|BIT(3)|BIT(4)|BIT(5))
#define SPI_EON_25Q16B_WRITE_PROTECT_STATUS     (BIT(2)|BIT(3)|BIT(4)|BIT(5))
#define SPI_GD25Q32_FLASH_WRITE_PROTECT_STATUS  (BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6))
#define SPI_FLASH_RDSR2                         (0x35)
#define SPI_FLASH_PROTECT_STATUS                (BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(14))

#define FLASH_INTR_DECLARE(t)
#define FLASH_INTR_LOCK(t)                      vPortEnterCritical()
#define FLASH_INTR_UNLOCK(t)                    vPortExitCritical()

#define FLASH_ALIGN_BYTES                       (4)
#define FLASH_ALIGN(addr)                       ((((size_t)addr) + (FLASH_ALIGN_BYTES - 1)) & (~(FLASH_ALIGN_BYTES - 1)))
#define FLASH_ALIGN_BEFORE(addr)                (FLASH_ALIGN(addr) - 4)
#define NOT_ALIGN(addr)                         (((size_t)addr) & (FLASH_ALIGN_BYTES - 1))
#define IS_ALIGN(addr)                          (NOT_ALIGN(addr) == 0)

#ifndef FLASH_SIZE
#define FLASH_SIZE                              (1)
#endif

#define CONFIG_SPI_FLASH_SIZE                   (FLASH_SIZE * 1048576)

enum GD25Q32C_status {
    GD25Q32C_STATUS1=0,
    GD25Q32C_STATUS2,
    GD25Q32C_STATUS3,
};

typedef enum {
    SPI_TX   = 0x1,
    SPI_RX   = 0x2,
    SPI_WRSR = 0x4,
    SPI_RAW  = 0x8,  /*!< No wait spi idle and no read status */
} spi_cmd_dir_t;

typedef struct {
    uint16_t cmd;
    uint8_t cmd_len;
    uint32_t *addr;
    uint8_t addr_len;
    uint32_t *data;
    uint8_t data_len;
    uint8_t dummy_bits;
} spi_cmd_t;
/*
sdk_flashchip_t sdk_flashchip = {
    0x1640ef,
    CONFIG_SPI_FLASH_SIZE,
    64 * 1024,
    4 * 1024,
    256,
    0xffff
};
*/
static void IRAM Cache_Read_Disable_2()
{
    CLEAR_PERI_REG_MASK(CACHE_FLASH_CTRL_REG, CACHE_READ_EN_BIT);
    while(REG_READ(SPI_EXT2(0)) != 0) { }
    CLEAR_PERI_REG_MASK(PERIPHS_SPI_FLASH_CTRL, SPI_CMD_ENABLE_AHB);
}

static void IRAM Cache_Read_Enable_2()
{
    SET_PERI_REG_MASK(PERIPHS_SPI_FLASH_CTRL, SPI_CMD_ENABLE_AHB);
    SET_PERI_REG_MASK(CACHE_FLASH_CTRL_REG, CACHE_READ_EN_BIT);
}
static void IRAM Cache_Read_Enable_New(void) __attribute__((alias("Cache_Read_Enable_2")));

static bool IRAM spi_user_cmd_raw(sdk_flashchip_t *chip, spi_cmd_dir_t mode, spi_cmd_t *p_cmd)
{
    int idx = 0;

    // Cache Disable
    Cache_Read_Disable_2();
    //wait spi idle
    if ((mode & SPI_RAW) == 0) {
        Wait_SPI_Idle(chip);
    }
    //save reg
    uint32_t io_mux_reg = READ_PERI_REG(PERIPHS_IO_MUX_CONF_U);
    uint32_t spi_clk_reg = READ_PERI_REG(SPI_CLOCK(SPI_ALT));
    uint32_t spi_ctrl_reg = READ_PERI_REG(SPI_CTRL(SPI_ALT));
    uint32_t spi_user_reg = READ_PERI_REG(SPI_USER(SPI_ALT));

    if (mode & SPI_WRSR) {
        // enable write register
        SPI_write_enable(chip);
    }

    SET_PERI_REG_MASK(SPI_USER(SPI_ALT),SPI_USR_COMMAND);

    //Disable flash operation mode
    CLEAR_PERI_REG_MASK(SPI_USER(SPI_ALT), SPI_FLASH_MODE);
    //SET SPI SEND BUFFER MODE
    CLEAR_PERI_REG_MASK(SPI_USER(SPI_ALT), SPI_USR_MISO_HIGHPART);
    //CLEAR EQU SYS CLK
    CLEAR_PERI_REG_MASK(PERIPHS_IO_MUX_CONF_U,SPI0_CLK_EQU_SYS_CLK);

    SET_PERI_REG_MASK(SPI_USER(SPI_ALT), SPI_CS_SETUP|SPI_CS_HOLD|SPI_USR_COMMAND|SPI_USR_MOSI);
    CLEAR_PERI_REG_MASK(SPI_USER(SPI_ALT), SPI_FLASH_MODE);
    //CLEAR DAUL OR QUAD LINES TRANSMISSION MODE
    CLEAR_PERI_REG_MASK(SPI_CTRL(SPI_ALT), SPI_QIO_MODE|SPI_DIO_MODE|SPI_DOUT_MODE|SPI_QOUT_MODE);
    WRITE_PERI_REG(SPI_CLOCK(SPI_ALT),
                   ((3&SPI_CLKCNT_N)<<SPI_CLKCNT_N_S)|
                   ((1&SPI_CLKCNT_H)<<SPI_CLKCNT_H_S)|
                   ((3&SPI_CLKCNT_L)<<SPI_CLKCNT_L_S)); //clear bit 31,set SPI clock div
    //Enable fast read mode
    SET_PERI_REG_MASK(SPI_CTRL(SPI_ALT), SPI_FASTRD_MODE);

    //WAIT COMMAND DONE
    while(READ_PERI_REG(SPI_CMD(SPI_ALT)) & SPI_USR);

    //SET USER CMD
    if (p_cmd->cmd_len != 0) {
        //Max CMD length is 16 bits
        SET_PERI_REG_BITS(SPI_USER2(SPI_ALT), SPI_USR_COMMAND_BITLEN, p_cmd->cmd_len * 8 - 1, SPI_USR_COMMAND_BITLEN_S);
        //Enable CMD
        SET_PERI_REG_MASK(SPI_USER(SPI_ALT), SPI_USR_COMMAND);
        //LOAD CMD
        SET_PERI_REG_BITS(SPI_USER2(SPI_ALT), SPI_USR_COMMAND_VALUE, p_cmd->cmd, SPI_USR_COMMAND_VALUE_S);
    } else {
        //CLEAR CMD
        CLEAR_PERI_REG_MASK(SPI_USER(SPI_ALT), SPI_USR_COMMAND);
        SET_PERI_REG_BITS(SPI_USER2(SPI_ALT), SPI_USR_COMMAND_BITLEN, 0, SPI_USR_COMMAND_BITLEN_S);
    }
    if (p_cmd->dummy_bits != 0) {
        //SET dummy bits
        SET_PERI_REG_BITS(SPI_USER1(SPI_ALT), SPI_USR_DUMMY_CYCLELEN, p_cmd->dummy_bits - 1, SPI_USR_DUMMY_CYCLELEN_S);
        //Enable dummy
        SET_PERI_REG_MASK(SPI_USER(SPI_ALT), SPI_USR_DUMMY);
    } else {
        //CLEAR DUMMY
        SET_PERI_REG_BITS(SPI_USER1(SPI_ALT), SPI_USR_DUMMY_CYCLELEN, 0, SPI_USR_DUMMY_CYCLELEN_S);
        CLEAR_PERI_REG_MASK(SPI_USER(SPI_ALT), SPI_USR_DUMMY);
    }

    //SET USER ADDRESS
    if (p_cmd->addr_len != 0) {
        //Set addr lenght
        SET_PERI_REG_BITS(SPI_USER1(SPI_ALT), SPI_USR_ADDR_BITLEN, p_cmd->addr_len * 8 - 1, SPI_USR_ADDR_BITLEN_S);
        //Enable user address
        SET_PERI_REG_MASK(SPI_USER(SPI_ALT), SPI_USR_ADDR);
        WRITE_PERI_REG(SPI_ADDR(SPI_ALT), *p_cmd->addr);
    } else {
        //CLEAR ADDR
        CLEAR_PERI_REG_MASK(SPI_USER(SPI_ALT), SPI_USR_ADDR);
        SET_PERI_REG_BITS(SPI_USER1(SPI_ALT), SPI_USR_ADDR_BITLEN, 0, SPI_USR_ADDR_BITLEN_S);
    }

    uint32_t *value = p_cmd->data;
    if (((mode & SPI_TX) || (mode & SPI_WRSR)) && p_cmd->data_len != 0) {
        //Enable MOSI, disable MISO
        SET_PERI_REG_MASK(SPI_USER(SPI_ALT), SPI_USR_MOSI);
        CLEAR_PERI_REG_MASK(SPI_USER(SPI_ALT), SPI_USR_MISO);
        do {
            WRITE_PERI_REG((SPI_W0(SPI_ALT) + (idx << 2)), *value++);
        } while ( ++idx < (p_cmd->data_len / 4));
        SET_PERI_REG_BITS(SPI_USER1(SPI_ALT), SPI_USR_MOSI_BITLEN, ((p_cmd->data_len) * 8 - 1), SPI_USR_MOSI_BITLEN_S);

    } else if ((mode & SPI_RX) && p_cmd->data_len != 0) {
        //Enable MISO, disable MOSI
        CLEAR_PERI_REG_MASK(SPI_USER(SPI_ALT), SPI_USR_MOSI);
        SET_PERI_REG_MASK(SPI_USER(SPI_ALT), SPI_USR_MISO);
        SET_PERI_REG_BITS(SPI_USER1(SPI_ALT),SPI_USR_MISO_BITLEN, p_cmd->data_len * 8 - 1, SPI_USR_MISO_BITLEN_S);
        int fifo_idx = 0;
        do {
            WRITE_PERI_REG((SPI_W0(SPI_ALT) + (fifo_idx << 2)), 0);
        } while ( ++fifo_idx < (p_cmd->data_len / 4));
    } else {
        CLEAR_PERI_REG_MASK(SPI_USER(SPI_ALT), SPI_USR_MOSI);
        CLEAR_PERI_REG_MASK(SPI_USER(SPI_ALT), SPI_USR_MISO);
        SET_PERI_REG_BITS(SPI_USER1(SPI_ALT), SPI_USR_MISO_BITLEN, 0, SPI_USR_MISO_BITLEN_S);
        SET_PERI_REG_BITS(SPI_USER1(SPI_ALT), SPI_USR_MOSI_BITLEN, 0, SPI_USR_MOSI_BITLEN_S);
    }

    //Start command
    SET_PERI_REG_MASK(SPI_CMD(SPI_ALT), SPI_USR);
    while (READ_PERI_REG(SPI_CMD(SPI_ALT)) & SPI_USR);

    if (mode & SPI_RX) {
        do {
            *p_cmd->data ++ = READ_PERI_REG(SPI_W0(SPI_ALT) + (idx << 2));
        } while (++idx < (p_cmd->data_len / 4));
    }

    //recover
    WRITE_PERI_REG(PERIPHS_IO_MUX_CONF_U,io_mux_reg);
    WRITE_PERI_REG(SPI_CTRL(SPI_ALT),spi_ctrl_reg);
    WRITE_PERI_REG(SPI_CLOCK(SPI_ALT),spi_clk_reg);
    WRITE_PERI_REG(SPI_USER(SPI_ALT),spi_user_reg);

    if ((mode & SPI_RAW) == 0) {
        Wait_SPI_Idle(chip);
    }
    //enable icache
    Cache_Read_Enable_2();

    return true;
}
/*
static uint32_t IRAM spi_flash_get_id_raw(sdk_flashchip_t *chip)
{
    uint32_t rdid = 0;

    Cache_Read_Disable();
    
    Wait_SPI_Idle(chip);

    WRITE_PERI_REG(PERIPHS_SPI_FLASH_C0, 0);    // clear regisrter
    WRITE_PERI_REG(PERIPHS_SPI_FLASH_CMD, SPI_CMD_READ_ID);
    while(READ_PERI_REG(PERIPHS_SPI_FLASH_CMD)!=0);

    rdid = READ_PERI_REG(PERIPHS_SPI_FLASH_C0)&0xffffff;

    Cache_Read_Enable_New();

    return rdid;
}
*/
static int IRAM spi_flash_read_status_raw(sdk_flashchip_t *chip, uint32_t *status)
{
    int ret;

    Cache_Read_Disable_2();

    ret = SPI_read_status(chip, status);

    Cache_Read_Enable_2();

    return ret;
}

static int IRAM spi_flash_write_status_raw(sdk_flashchip_t *chip, uint32_t status_value)
{
    Cache_Read_Disable_2();

    Wait_SPI_Idle(chip);

    if (SPI_write_enable(chip) != 0) {
        return -1;
    }

    if (SPI_write_status(chip, status_value) != 0) {
        return -1;
    }

    Wait_SPI_Idle(chip);

    Cache_Read_Enable_2();

    return 0;
}


static bool IRAM spi_user_cmd(spi_cmd_dir_t mode, spi_cmd_t *p_cmd)
{
    bool ret;
    FLASH_INTR_DECLARE(c_tmp);

    if ((p_cmd->addr_len != 0 && p_cmd->addr == NULL)
            || (p_cmd->data_len != 0 && p_cmd->data == NULL)
            || (p_cmd == NULL)) {
        return false;
    }

    FLASH_INTR_LOCK(c_tmp);

    ret = spi_user_cmd_raw(&sdk_flashchip, mode, p_cmd);

    FLASH_INTR_UNLOCK(c_tmp);

    return ret;
}

static bool IRAM special_flash_read_status(uint8_t command, uint32_t* status, int len)
{
    bool ret;
    spi_cmd_t cmd;

    if ( len > 2 || len < 1) {
        return false;
    }

    cmd.cmd = command;
    cmd.cmd_len = 1;
    cmd.addr = NULL;
    cmd.addr_len = 0;
    cmd.dummy_bits = 0;
    cmd.data = status;
    cmd.data_len = len;

    ret = spi_user_cmd(SPI_RX, &cmd);

    return ret;
}

static bool IRAM special_flash_write_status(uint8_t command, uint32_t status, int len, bool write_en)
{
    bool ret;
    spi_cmd_t cmd;

    if (len > 2 || len < 1) {
        return false;
    }

    cmd.cmd = command;
    cmd.cmd_len = 1;
    cmd.addr = NULL;
    cmd.addr_len = 0;
    cmd.dummy_bits = 0;
    cmd.data = &status;
    cmd.data_len = len > 1 ? 2 : 1;
    if (write_en) {
        ret = spi_user_cmd(SPI_WRSR, &cmd);
    } else {
        ret = spi_user_cmd(SPI_TX | SPI_RAW, &cmd);
    }

    return ret;
}

static uint8_t IRAM en25q16x_read_sfdp()
{
    spi_cmd_t cmd;
    uint32_t addr = 0x00003000;
    uint32_t data = 0;

    cmd.cmd = 0x5a;
    cmd.cmd_len = 1;

    cmd.addr = &addr;
    cmd.addr_len = 3;
    cmd.dummy_bits = 8;

    cmd.data = &data;
    cmd.data_len = 1;

    spi_user_cmd(SPI_RX, &cmd);

    return ((uint8_t) data);
}
/*
static uint32_t IRAM spi_flash_get_id(void)
{
    uint32_t rdid = 0;
    FLASH_INTR_DECLARE(c_tmp);

    FLASH_INTR_LOCK(c_tmp);

    rdid = spi_flash_get_id_raw(&sdk_flashchip);

    FLASH_INTR_UNLOCK(c_tmp);

    return rdid;
}
*/
static int IRAM spi_flash_read_status(uint32_t *status)
{
    int ret;
    FLASH_INTR_DECLARE(c_tmp);

    FLASH_INTR_LOCK(c_tmp);

    ret = spi_flash_read_status_raw(&sdk_flashchip, status);

    FLASH_INTR_UNLOCK(c_tmp);

    return ret;
}

static void IRAM spi_flash_write_status(uint32_t status_value)
{
    FLASH_INTR_DECLARE(c_tmp);

    FLASH_INTR_LOCK(c_tmp);

    spi_flash_write_status_raw(&sdk_flashchip, status_value);

    FLASH_INTR_UNLOCK(c_tmp);
}

static bool IRAM spi_flash_check_wr_protect()
{
    uint32_t flash_id = sdk_spi_flash_get_id();
    uint32_t status = 0;
    //check for EN25Q16A/B flash chips
    
    if ((flash_id & 0xffffff) == 0x15701c) {
        uint8_t sfdp = en25q16x_read_sfdp();
        if (sfdp == 0xE5) {
            //This is EN25Q16A, set bit6 in the same way as issi flash chips.
            if (spi_flash_read_status(&status) == 0) { //Read status Ok
                if (status & (SPI_EON_25Q16A_WRITE_PROTECT_STATUS)) { //Write_protect
                    special_flash_write_status(0x1, status & (~(SPI_EON_25Q16A_WRITE_PROTECT_STATUS)), 1, true);
                }
            }
        } else if (sfdp == 0xED) {
            //This is EN25Q16B
            if (spi_flash_read_status(&status) == 0) { //Read status Ok
                if (status & (SPI_EON_25Q16B_WRITE_PROTECT_STATUS)) { //Write_protect
                    special_flash_write_status(0x1, status & (~(SPI_EON_25Q16B_WRITE_PROTECT_STATUS)), 1, true);
                }
            }
        }
    }
    //MXIC :0XC2
    //ISSI :0X9D
    // ets_printf("spi_flash_check_wr_protect\r\n");
    else if (((flash_id & 0xFF) == 0X9D)||((flash_id & 0xFF) == 0XC2)||((flash_id & 0xFF) == 0x1C)) {
        if (spi_flash_read_status(&status) == 0) { //Read status Ok
            if (status & (SPI_ISSI_FLASH_WRITE_PROTECT_STATUS)) { //Write_protect
                special_flash_write_status(0x1, status & (~(SPI_ISSI_FLASH_WRITE_PROTECT_STATUS)), 1, true);
            }
        }
    }
    //GD25Q32C:0X16409D
    //GD25Q128
    else if (((flash_id & 0xFFFFFFFF) == 0X1640C8) || ((flash_id & 0xFFFFFFFF) == 0X1840C8)) {
        if (spi_flash_read_status(&status) == 0) { //Read status Ok
            if (status & SPI_GD25Q32_FLASH_WRITE_PROTECT_STATUS) {
                special_flash_write_status(0x01, status & (~(SPI_GD25Q32_FLASH_WRITE_PROTECT_STATUS)), 1, true);
            }
        }
    } else if (flash_id == 0x164068) {
        if (spi_flash_read_status(&status) == 0) { //Read status Ok
            if (status&SPI_GD25Q32_FLASH_WRITE_PROTECT_STATUS) {
                special_flash_write_status(0x01, status & (~(SPI_GD25Q32_FLASH_WRITE_PROTECT_STATUS)), 1, true);
            }
        }
    }
    //Others
    else {
        if (spi_flash_read_status(&status) == 0) { //Read status Ok
            uint32_t status1 = 0; //flash_gd25q32c_read_status(GD25Q32C_STATUS2);
            special_flash_read_status(SPI_FLASH_RDSR2, &status1, 1);
            status = (status1 << 8) | (status & 0xff);
            if (status & SPI_FLASH_PROTECT_STATUS) {
                status=((status & (~SPI_FLASH_PROTECT_STATUS)) & 0xffff);
                spi_flash_write_status(status);
            }
        }
    }
    
    return true;
}
/*
int IRAM spi_flash_enable_qmode_raw(sdk_flashchip_t *chip)
{
    int ret;

    Cache_Read_Disable_2();

    ret = Enable_QMode(chip);

    Wait_SPI_Idle(chip);

    Cache_Read_Enable_2();

    return ret;
}

void IRAM spi_flash_switch_to_qio_raw(void)
{
    CLEAR_PERI_REG_MASK(PERIPHS_SPI_FLASH_CTRL, SPI_QIO_MODE
                        |SPI_QOUT_MODE
                        |SPI_DIO_MODE
                        |SPI_DOUT_MODE
                        |SPI_FASTRD_MODE);

    SET_PERI_REG_MASK(PERIPHS_SPI_FLASH_CTRL, SPI_QIO_MODE | SPI_FASTRD_MODE);
}

static void IRAM spi_flash_enable_qio_bit6(void)
{
    uint8_t wrsr_cmd = 0x1;
    uint32_t issi_qio = SPI_FLASH_ISSI_ENABLE_QIO_MODE;
    special_flash_write_status(wrsr_cmd, issi_qio, 1, true);
}

static bool IRAM spi_flash_issi_enable_QIO_mode(void)
{
    uint32_t status = 0;
    if(spi_flash_read_status(&status) == 0) {
        if((status&SPI_FLASH_ISSI_ENABLE_QIO_MODE)) {
            return true;
        }
    }
    else {
        return false;
    }

    spi_flash_enable_qio_bit6();

    if(spi_flash_read_status(&status) == 0) {
        if((status&SPI_FLASH_ISSI_ENABLE_QIO_MODE)) {
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

static uint8_t IRAM flash_gd25q32c_read_status(enum GD25Q32C_status status_index)
{
    uint8_t rdsr_cmd=0;
    if(GD25Q32C_STATUS1 == status_index) {
        rdsr_cmd = SPI_FLASH_GD25Q32C_READ_STATUSE1_CMD;
    }
    else if(GD25Q32C_STATUS2 == status_index) {
        rdsr_cmd = SPI_FLASH_GD25Q32C_READ_STATUSE2_CMD;
    }
    else if(GD25Q32C_STATUS3 == status_index) {
        rdsr_cmd = SPI_FLASH_GD25Q32C_READ_STATUSE3_CMD;
    }
    else {

    }
    uint32_t status;
    special_flash_read_status(rdsr_cmd, &status, 1);
    return ((uint8_t)status);
}

static void IRAM flash_gd25q32c_write_status(enum GD25Q32C_status status_index,uint8_t status)
{
    uint32_t wrsr_cmd=0;
    uint32_t new_status = status;
    if(GD25Q32C_STATUS1 == status_index) {
        wrsr_cmd = SPI_FLASH_GD25Q32C_WRITE_STATUSE1_CMD;
    }
    else if(GD25Q32C_STATUS2 == status_index) {
        wrsr_cmd = SPI_FLASH_GD25Q32C_WRITE_STATUSE2_CMD;
    }
    else if(GD25Q32C_STATUS3 == status_index) {
        wrsr_cmd = SPI_FLASH_GD25Q32C_WRITE_STATUSE3_CMD;
    }
    else {
        //ets_printf("[ERR]Not konw GD25Q32C status idx %d\n ",spi_wr_status_cmd);
    }
    special_flash_write_status(wrsr_cmd, new_status, 1, true);
}

static bool IRAM flash_gd25q32c_enable_QIO_mode()
{
    uint8_t data = 0;
    if((data=flash_gd25q32c_read_status(GD25Q32C_STATUS2))&SPI_FLASH_GD25Q32C_QIO_MODE) {
        return true;
    }
    else {
        flash_gd25q32c_write_status(GD25Q32C_STATUS2,SPI_FLASH_GD25Q32C_QIO_MODE);
        if(flash_gd25q32c_read_status(GD25Q32C_STATUS2)&SPI_FLASH_GD25Q32C_QIO_MODE) {
            return true;
        }
        else {
            return false;
        }
    }
}

void IRAM special_flash_set_mode(uint8_t command, bool disable_wait_idle)
{
    spi_cmd_t cmd;
    cmd.cmd = command;
    cmd.cmd_len = 1;
    cmd.addr = NULL;
    cmd.addr_len = 0;
    cmd.dummy_bits = 0;
    cmd.data = NULL;
    cmd.data_len = 0;
    if (disable_wait_idle) {
        spi_user_cmd(SPI_TX | SPI_RAW, &cmd);
    } else {
        spi_user_cmd(SPI_TX, &cmd);
    }
}

static bool IRAM en25q16x_write_volatile_status(uint8_t vsr)
{
    //enter OTP mode
    special_flash_set_mode(0x3a, true);
    //volatile status register write enable
    special_flash_set_mode(0x50, true);
    //send 0x01 + 0x40 to set WHDIS bit
    special_flash_write_status(0x01, vsr, 1, false);
    //check
    uint32_t status = 0;
    special_flash_read_status(0x05, &status, 1);
    //Leave OTP mode
    special_flash_set_mode(0x04, false);

    if (status == 0x40) {
        return true;
    } else {
        return false;
    }
}

void IRAM spi_flash_switch_to_dout_raw()
{
    CLEAR_PERI_REG_MASK(PERIPHS_SPI_FLASH_CTRL, SPI_QIO_MODE
                        |SPI_QOUT_MODE
                        |SPI_DIO_MODE
                        |SPI_DOUT_MODE
                        |SPI_FASTRD_MODE);

    SET_PERI_REG_MASK(PERIPHS_SPI_FLASH_CTRL, SPI_DOUT_MODE);
}

void IRAM user_spi_flash_dio_to_qio_pre_init()
{
    spi_flash_switch_to_dout_raw();
    
    uint32_t flash_id = sdk_spi_flash_get_id();
    bool to_qio = false;
    //check for EN25Q16A/B flash chips
    if ((flash_id & 0xffffff) == 0x15701c) {
        uint8_t sfdp = en25q16x_read_sfdp();
        if (sfdp == 0xE5) {
            //This is EN25Q16A, set bit6 in the same way as issi flash chips.
            if (spi_flash_issi_enable_QIO_mode() == true) {
                to_qio = true;
            }
        } else if (sfdp == 0xED) {
            //This is EN25Q16B
            if (en25q16x_write_volatile_status(0x40) == true) {
                to_qio = true;
            }
        }
    }
    // ISSI : 0x9D
    // MXIC : 0xC2
    // GD25Q32C & GD25Q128C : 0x1640C8
    // EON : 0X1C
    // ENABLE FLASH QIO 0X01H+BIT6
    else if (((flash_id & 0xFF) == 0x9D) || ((flash_id & 0xFF) == 0xC2) || ((flash_id & 0xFF) == 0x1C) ) {
        if (spi_flash_issi_enable_QIO_mode() == true) {
            to_qio = true;
        }
        //ENABLE FLASH QIO 0X31H+BIT2
    } else if ((flash_id & 0xFFFF) == 0x40C8) {
        if (flash_gd25q32c_enable_QIO_mode() == true) {
            to_qio = true;
        }
        //ENBALE FLASH QIO 0X01H+0X00+0X02
    } else if (flash_id == 0x164068) {
        if (flash_gd25q32c_enable_QIO_mode() == true) {
            to_qio = true;
        }
    } else {
        if (spi_flash_enable_qmode_raw(&sdk_flashchip) == 0) {
            to_qio = true;
        }
    }
    
    if (to_qio == true) {
        spi_flash_switch_to_qio_raw();
    }
}

*/
/**
 * Low level SPI flash write. Write block of data up to 64 bytes.
 */
static inline void IRAM spi_write_data(sdk_flashchip_t *chip, uint32_t addr,
        uint8_t *buf, uint32_t size)
{
    uint32_t words = size >> 2;
    if (size & 0b11) {
        words++;
    }

    Wait_SPI_Idle(chip);  // wait for previous write to finish

    SPI(0).ADDR = (addr & 0x00FFFFFF) | (size << 24);

    memcpy((void*)SPI(0).W, buf, words<<2);

    __asm__ volatile("memw");

    SPI_write_enable(chip);

    SPI(0).CMD = SPI_CMD_PP;
    while (SPI(0).CMD) {}
}

/**
 * Write a page of flash. Data block should not cross page boundary.
 */
static bool IRAM spi_write_page(sdk_flashchip_t *flashchip, uint32_t dest_addr,
    uint8_t *buf, uint32_t size)
{
    // check if block to write doesn't cross page boundary
    if (flashchip->page_size < size + (dest_addr % flashchip->page_size)) {
        return false;
    }

    if (size < 1) {
        return true;
    }

    while (size >= SPI_WRITE_MAX_SIZE) {
        spi_write_data(flashchip, dest_addr, buf, SPI_WRITE_MAX_SIZE);

        size -= SPI_WRITE_MAX_SIZE;
        dest_addr += SPI_WRITE_MAX_SIZE;
        buf += SPI_WRITE_MAX_SIZE;

        if (size < 1) {
            return true;
        }
    }

    spi_write_data(flashchip, dest_addr, buf, size);

    return true;
}

/**
 * Split block of data into pages and write pages.
 */
static bool IRAM spi_write(uint32_t addr, uint8_t *dst, uint32_t size)
{
    if (sdk_flashchip.chip_size < (addr + size)) {
        return false;
    }

    uint32_t write_bytes_to_page = sdk_flashchip.page_size -
        (addr % sdk_flashchip.page_size);  // TODO: place for optimization

    if (size < write_bytes_to_page) {
        if (!spi_write_page(&sdk_flashchip, addr, dst, size)) {
            return false;
        }
    } else {
        if (!spi_write_page(&sdk_flashchip, addr, dst, write_bytes_to_page)) {
            return false;
        }

        uint32_t offset = write_bytes_to_page;
        uint32_t pages_to_write = (size - offset) / sdk_flashchip.page_size;
        for (uint32_t i = 0; i < pages_to_write; i++) {
            if (!spi_write_page(&sdk_flashchip, addr + offset,
                        dst + offset, sdk_flashchip.page_size)) {
                return false;
            }
            offset += sdk_flashchip.page_size;
        }

        if (!spi_write_page(&sdk_flashchip, addr + offset,
                    dst + offset, size - offset)) {
            return false;
        }
    }

    return true;
}

bool IRAM spiflash_write(uint32_t addr, uint8_t *buf, uint32_t size)
{
    bool result = false;

    if (buf) {
        if (spi_flash_check_wr_protect(&sdk_flashchip) == false) {
            return false;
        }
        
        vPortEnterCritical();
        Cache_Read_Disable();

        result = spi_write(addr, buf, size);

        // make sure all write operations is finished before exiting
        Wait_SPI_Idle(&sdk_flashchip);

        Cache_Read_Enable(0, 0, 1);
        vPortExitCritical();
    }

    return result;
}

/**
 * Read SPI flash up to 64 bytes.
 */
static inline void IRAM read_block(sdk_flashchip_t *chip, uint32_t addr,
        uint8_t *buf, uint32_t size)
{
    SPI(0).ADDR = (addr & 0x00FFFFFF) | (size << 24);
    SPI(0).CMD = SPI_CMD_READ;

    while (SPI(0).CMD) {};

    __asm__ volatile("memw");

    memcpy(buf, (const void*)SPI(0).W, size);
}

/**
 * Read SPI flash data. Data region doesn't need to be page aligned.
 */
static inline bool IRAM read_data(sdk_flashchip_t *flashchip, uint32_t addr,
        uint8_t *dst, uint32_t size)
{
    if (size < 1) {
        return true;
    }

    if ((addr + size) > flashchip->chip_size) {
        return false;
    }

    while (size >= SPI_READ_MAX_SIZE) {
        read_block(flashchip, addr, dst, SPI_READ_MAX_SIZE);
        dst += SPI_READ_MAX_SIZE;
        size -= SPI_READ_MAX_SIZE;
        addr += SPI_READ_MAX_SIZE;
    }

    if (size > 0) {
        read_block(flashchip, addr, dst, size);
    }

    return true;
}

bool IRAM spiflash_read(uint32_t dest_addr, uint8_t *buf, uint32_t size)
{
    bool result = false;

    if (buf) {
        vPortEnterCritical();
        Cache_Read_Disable();

        result = read_data(&sdk_flashchip, dest_addr, buf, size);

        Cache_Read_Enable(0, 0, 1);
        vPortExitCritical();
    }

    return result;
}

/* Original
bool IRAM spiflash_erase_sector(uint32_t addr)
{
    if ((addr + sdk_flashchip.sector_size) > sdk_flashchip.chip_size) {
        return false;
    }

    if (addr & 0xFFF) {
        return false;
    }
    
    if (spi_flash_check_wr_protect(&sdk_flashchip) == false) {
        return false;
    }

    vPortEnterCritical();
    Cache_Read_Disable();
    
    SPI_write_enable(&sdk_flashchip);

    SPI(0).ADDR = addr & 0x00FFFFFF;
    SPI(0).CMD = SPI_CMD_SE;
    while (SPI(0).CMD) {};

    Wait_SPI_Idle(&sdk_flashchip);

    Cache_Read_Enable(0, 0, 1);
    vPortExitCritical();

    return true;
}
*/

bool IRAM spiflash_erase_sector(uint32_t addr)
{
    int ret = true;
    
    if ((addr + sdk_flashchip.sector_size) > sdk_flashchip.chip_size) {
        return false;
    }

    if (addr & 0xFFF) {
        return false;
    }
    
    if (spi_flash_check_wr_protect(&sdk_flashchip) == false) {
        return false;
    }

    vPortEnterCritical();

    Cache_Read_Disable_2();

    if (SPI_write_enable(&sdk_flashchip) != 0) {
        ret = false;
    }
    
    if (SPI_sector_erase(&sdk_flashchip, addr) != 0) {
        ret = false;
    }

    Cache_Read_Enable_2();
    
    vPortExitCritical();

    return ret;
}
