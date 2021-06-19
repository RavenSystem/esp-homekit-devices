/**
 * Driver for serial nonvolatile ferroelectric random access
 * memory or F-RAM.
 *
 * Part of esp-open-rtos
 * Copyright (C) 2017 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#ifndef EXTRAS_FRAM_H_
#define EXTRAS_FRAM_H_

#include <esp/spi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FRAM_ID_LEN 9

/**
 * F-RAM device descriptor
 */
typedef struct
{
    uint8_t spi_bus;       //!< SPI bus
    uint8_t cs_gpio;       //!< chip select GPIO
    uint32_t spi_freq_div; //!< SPI bus frequency divider
} fram_t;

/**
 * F-RAM device ID
 */
typedef struct
{
    uint8_t data[FRAM_ID_LEN];
    union
    {
        uint8_t rsvd:    3;
        uint8_t rev:     3;
        uint8_t sub:     2;
        uint8_t density: 5;
        uint8_t family:  3;
        uint8_t manufacturer[FRAM_ID_LEN - 2];
    };
} fram_id_t;


/**
 * Write protection mode
 */
typedef enum
{
    FRAM_WP_NONE = 0,     //!< No write protection
    FRAM_WP_UPPER_QUARTER,//!< Upper 1/4 write protection
    FRAM_WP_UPPER_HALF,   //!< Upper 1/2 write protection
    FRAM_WP_ALL           //!< All memory write protection
} fram_wp_mode_t;

/**
 * Prepare to read/write F-RAM
 * @param dev Pointer to device descriptor
 */
void fram_init(const fram_t *dev);

/**
 * Read data from F-RAM
 * @param dev Pointer to device descriptor
 * @param to Buffer to store data
 * @param from F-RAM address
 * @param size Bytes to read
 */
void fram_read(const fram_t *dev, void *to, void *from, size_t size);

/**
 * Write data to F-RAM
 * @param dev Pointer to device descriptor
 * @param from Data buffer
 * @param to F-RAM address
 * @param size Bytes to write
 */
void fram_write(const fram_t *dev, void *from, void *to, size_t size);

/**
 * Set device to sleep mode or wake up
 * @param dev Pointer to device descriptor
 * @param sleep Set to sleep mode when true, wake up otherwise
 */
void fram_sleep(const fram_t *dev, bool sleep);

/**
 * Check if F-RAM busy with another SPI master.
 * To use this method of sharing F-RAM between two masters
 * you'll need to pull up CS GPIO line
 * @param dev Pointer to device descriptor
 * @return true when device is busy
 */
bool fram_busy(const fram_t *dev);

/**
 * Read the device ID
 * @param dev Pointer to device descriptor
 * @param id Poiner to device ID structure
 */
void fram_id(const fram_t *dev, fram_id_t *id);

/**
 * Set block write protection mode
 * @param dev Pointer to device descriptor
 * @param mode Write protection mode
 */
void fram_set_wp_mode(const fram_t *dev, fram_wp_mode_t mode);

/**
 * Get current write protection mode
 * @param dev Pointer to device descriptor
 * @return Write protection mode
 */
fram_wp_mode_t fram_get_wp_mode(const fram_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* EXTRAS_FRAM_H_ */
