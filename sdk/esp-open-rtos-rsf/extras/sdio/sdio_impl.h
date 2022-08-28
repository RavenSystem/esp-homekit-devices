/*
 * Hardware SPI driver for MMC/SD/SDHC cards
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#ifndef _EXTRAS_SDIO_IMPL_H_
#define _EXTRAS_SDIO_IMPL_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef union
{
   uint32_t data;
   struct
   {
       uint16_t reserverd1 :15;
       uint8_t vcc_2v7_2v8 :1;
       uint8_t vcc_2v8_2v9 :1;
       uint8_t vcc_2v9_3v0 :1;
       uint8_t vcc_3v0_3v1 :1;
       uint8_t vcc_3v1_3v2 :1;
       uint8_t vcc_3v2_3v3 :1;
       uint8_t vcc_3v3_3v4 :1;
       uint8_t vcc_3v4_3v5 :1;
       uint8_t vcc_3v5_3v6 :1;
       uint8_t reserverd2  :6;
       uint8_t ccs         :1;
       uint8_t busy        :1;
   } __attribute__((packed)) bits;
} sdio_ocr_t;

typedef union
{
    uint8_t data[16];
    struct
    {
        uint8_t  manufacturer_id;
        char     oem_id[2];
        char     product_name[5];
        uint8_t  product_rev_minor :4;
        uint8_t  product_rev_major :4;
        uint32_t product_serial;
        uint8_t  reserved1         :4;
        uint8_t  date_year_h       :4;
        uint8_t  date_year_l       :4;
        uint8_t  date_month        :4;
        uint8_t  always1           :1;
        uint8_t  crc               :7;
    } __attribute__((packed)) bits;
} sdio_cid_t;

typedef union
{
    uint8_t data[16];
    struct
    {
        uint8_t reserved1          :6;
        uint8_t csd_ver            :2;
        uint8_t taac;
        uint8_t nsac;
        uint8_t tran_speed;
        uint8_t ccc_high;
        uint8_t read_bl_len        :4;
        uint8_t ccc_low            :4;
        uint16_t c_size_high       :2;
        uint8_t reserved2          :2;
        uint8_t dsr_imp            :1;
        uint8_t read_blk_misalign  :1;
        uint8_t write_blk_misalign :1;
        uint8_t read_bl_partial    :1;
        uint8_t c_size_mid;
        uint8_t vdd_r_curr_max     :3;
        uint8_t vdd_r_curr_min     :3;
        uint8_t c_size_low         :2;
        uint8_t c_size_mult_high   :2;
        uint8_t vdd_w_cur_max      :3;
        uint8_t vdd_w_curr_min     :3;
        uint8_t sector_size_high   :6;
        uint8_t erase_blk_en       :1;
        uint8_t c_size_mult_low    :1;
        uint8_t wp_grp_size        :7;
        uint8_t sector_size_low    :1;
        uint8_t write_bl_len_high  :2;
        uint8_t r2w_factor         :3;
        uint8_t reserved3          :2;
        uint8_t wp_grp_enable      :1;
        uint8_t reserved4          :5;
        uint8_t write_partial      :1;
        uint8_t write_bl_len_low   :2;
        uint8_t reserved5          :2;
        uint8_t file_format        :2;
        uint8_t tmp_write_protect  :1;
        uint8_t perm_write_protect :1;
        uint8_t copy               :1;
        uint8_t file_format_grp    :1;
        uint8_t always1            :1;
        uint8_t crc                :7;
    } __attribute__((packed)) v1;

    struct
    {
        uint8_t reserved1          :6;
        uint8_t csd_ver            :2;
        uint8_t taac;
        uint8_t nsac;
        uint8_t tran_speed;
        uint8_t ccc_high;
        uint8_t read_bl_len        :4;
        uint8_t ccc_low            :4;
        uint8_t reserved2          :4;
        uint8_t dsr_imp            :1;
        uint8_t read_blk_misalign  :1;
        uint8_t write_blk_misalign :1;
        uint8_t read_bl_partial    :1;
        uint16_t c_size_high       :6;
        uint8_t reserved3          :2;
        uint8_t c_size_mid;
        uint8_t c_size_low;
        uint8_t sector_size_high   :6;
        uint8_t erase_blk_en       :1;
        uint8_t reserved4          :1;
        uint8_t wp_grp_size        :7;
        uint8_t sector_size_low    :1;
        uint8_t write_bl_len_high  :2;
        uint8_t r2w_factor         :3;
        uint8_t reserved5          :2;
        uint8_t wp_grp_enable      :1;
        uint8_t reserved6          :5;
        uint8_t write_partial      :1;
        uint8_t write_bl_len_low   :2;
        uint8_t reserved7          :2;
        uint8_t file_format        :2;
        uint8_t tmp_write_protect  :1;
        uint8_t perm_write_protect :1;
        uint8_t copy               :1;
        uint8_t file_format_grp    :1;
        uint8_t always1            :1;
        uint8_t crc                :7;
    } __attribute__((packed)) v2;
} sdio_csd_t;


#ifdef __cplusplus
}
#endif


#endif /* _EXTRAS_SDIO_IMPL_H_ */
