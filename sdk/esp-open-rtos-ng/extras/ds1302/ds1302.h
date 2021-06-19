/*
 * Driver for DS1302 RTC
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>,
 *                    Pavel Merzlyakov <merzlyakovpavel@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#ifndef EXTRAS_DS1302_H_
#define EXTRAS_DS1302_H_

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DS1302_RAM_SIZE 31

void ds1302_init(uint8_t ce_pin, uint8_t io_pin, uint8_t sclk_pin);

/**
 * \brief Start/stop clock
 * \param start Start clock if true
 * \return False if RTC is write-protected
 */
bool ds1302_start(bool start);

/**
 * \brief Get current clock state
 * \return true if clock running
 */
bool ds1302_is_running();

/**
 * \brief Enable/disable write protection
 * \param protect Set RTC write-protected if true
 */
void ds1302_set_write_protect(bool protect);

/**
 * \brief Get write protection status
 * \return true if RTC write-protected
 */
bool ds1302_get_write_protect();

/**
 * \brief Get current time
 * \param time Pointer to the time struct to fill
 */
void ds1302_get_time(struct tm *time);

/**
 * \brief Set time to RTC
 * \param time Pointer to the time struct
 * \return False if RTC is write-protected
 */
bool ds1302_set_time(const struct tm *time);

/**
 * \brief Read RAM contents into the buffer
 * \param offset Start byte, 0..55
 * \param buf Buffer
 * \param len Bytes to read, 1..56
 * \return false if error occured (invalid offset or buffer too big)
 */
bool ds1302_read_sram(uint8_t offset, void *buf, uint8_t len);

/**
 * \brief Write buffer to RTC RAM
 * \param offset Start byte, 0..55
 * \param buf Buffer
 * \param len Bytes to write, 1..56
 * \return false if error occured (invalid offset or buffer too big)
 */
bool ds1302_write_sram(uint8_t offset, void *buf, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* EXTRAS_DS1302_H_ */
