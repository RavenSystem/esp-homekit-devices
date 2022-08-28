/**
 * \file Hardware SPI master driver
 *
 * Part of esp-open-rtos
 *
 * \copyright Ruslan V. Uss, 2016
 * BSD Licensed as described in the file LICENSE
 */
#ifndef _ESP_SPI_H_
#define _ESP_SPI_H_

#include <stdbool.h>
#include <stdint.h>
#include "esp/spi_regs.h"
#include "esp/clocks.h"

/**
 * Macro for use with spi_init and spi_set_frequency_div.
 * SPI frequency = 80000000 / divider / count
 * dvider must be in 1..8192 and count in 1..64
 */
#define SPI_GET_FREQ_DIV(divider, count) (((count) << 16) | ((divider) & 0xffff))

/**
 * Predefinded SPI frequency dividers
 */
#define SPI_FREQ_DIV_125K SPI_GET_FREQ_DIV(64, 10) ///< 125kHz
#define SPI_FREQ_DIV_250K SPI_GET_FREQ_DIV(32, 10) ///< 250kHz
#define SPI_FREQ_DIV_500K SPI_GET_FREQ_DIV(16, 10) ///< 500kHz
#define SPI_FREQ_DIV_1M   SPI_GET_FREQ_DIV(8, 10)  ///< 1MHz
#define SPI_FREQ_DIV_2M   SPI_GET_FREQ_DIV(4, 10)  ///< 2MHz
#define SPI_FREQ_DIV_4M   SPI_GET_FREQ_DIV(2, 10)  ///< 4MHz
#define SPI_FREQ_DIV_8M   SPI_GET_FREQ_DIV(5,  2)  ///< 8MHz
#define SPI_FREQ_DIV_10M  SPI_GET_FREQ_DIV(4,  2)  ///< 10MHz
#define SPI_FREQ_DIV_20M  SPI_GET_FREQ_DIV(2,  2)  ///< 20MHz
#define SPI_FREQ_DIV_40M  SPI_GET_FREQ_DIV(1,  2)  ///< 40MHz
#define SPI_FREQ_DIV_80M  SPI_GET_FREQ_DIV(1,  1)  ///< 80MHz

/*
 * Possible Data Structure of SPI Transaction
 *
 * [COMMAND]+[ADDRESS]+[DataOUT]+[DUMMYBITS]+[DataIN]
 *
 * [COMMAND]+[ADDRESS]+[DUMMYBITS]+[DataOUT]
 *
 */

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum _spi_mode_t {
    SPI_MODE0 = 0,  ///< CPOL = 0, CPHA = 0
    SPI_MODE1,      ///< CPOL = 0, CPHA = 1
    SPI_MODE2,      ///< CPOL = 1, CPHA = 0
    SPI_MODE3       ///< CPOL = 1, CPHA = 1
} spi_mode_t;

typedef enum _spi_endianness_t {
    SPI_LITTLE_ENDIAN = 0,
    SPI_BIG_ENDIAN
} spi_endianness_t;

typedef enum _spi_word_size_t {
    SPI_8BIT  = 1,  ///< 1 byte
    SPI_16BIT = 2,  ///< 2 bytes
    SPI_32BIT = 4   ///< 4 bytes
} spi_word_size_t;

/**
 * SPI bus settings
 */
typedef struct
{
    spi_mode_t mode;              ///< Bus mode
    uint32_t freq_divider;        ///< Bus frequency as a divider. See spi_init()
    bool msb;                     ///< MSB first if true
    spi_endianness_t endianness;  ///< Bus byte order
    bool minimal_pins;            ///< Minimal set of pins if true. Spee spi_init()
} spi_settings_t;

/**
 * \brief Initalize SPI bus
 * Initalize specified SPI bus and setup appropriate pins:
 * Bus 0:
 *   - MISO = GPIO 7
 *   - MOSI = GPIO 8
 *   - SCK  = GPIO 6
 *   - CS0  = GPIO 11 (if minimal_pins is false)
 *   - HD   = GPIO 9  (if minimal_pins is false)
 *   - WP   = GPIO 10 (if minimal_pins is false)
 * Bus 1:
 *   - MISO = GPIO 12
 *   - MOSI = GPIO 13
 *   - SCK  = GPIO 14
 *   - CS0  = GPIO 15 (if minimal_pins is false)
 * Note that system flash memory is on the bus 0!
 * \param bus Bus ID: 0 - system, 1 - user
 * \param mode Bus mode
 * \param freq_divider SPI bus frequency divider, use SPI_GET_FREQ_DIV() or predefined value
 * \param msb Bit order, MSB first if true
 * \param endianness Byte order
 * \param minimal_pins If true use the minimal set of pins: MISO, MOSI and SCK.
 * \return false when error
 */
bool spi_init(uint8_t bus, spi_mode_t mode, uint32_t freq_divider, bool msb, spi_endianness_t endianness, bool minimal_pins);
/**
 * \brief Initalize SPI bus
 * spi_init() wrapper.
 * Example:
 *
 *     const spi_settings_t my_settings = {
 *         .mode = SPI_MODE0,
 *         .freq_divider = SPI_FREQ_DIV_4M,
 *         .msb = true,
 *         .endianness = SPI_LITTLE_ENDIAN,
 *         .minimal_pins = true
 *     }
 *     ....
 *     spi_settings_t old;
 *     spi_get_settings(1, &old); // save current settings
 *     //spi_init(1, SPI_MODE0, SPI_FREQ_DIV_4M, true, SPI_LITTLE_ENDIAN, true); // use own settings
 *     // or
 *     spi_set_settings(1, &my_settings);
 *     // some work with spi here
 *     ....
 *     spi_set_settings(1, &old); // restore saved settings
 *
 * \param s Pointer to the settings structure
 * \return false when error
 */
static inline bool spi_set_settings(uint8_t bus, const spi_settings_t *s)
{
    return spi_init(bus, s->mode, s->freq_divider, s->msb, s->endianness, s->minimal_pins);
}
/**
 * \brief Get current settings of the SPI bus
 * See spi_set_settings().
 * \param bus Bus ID: 0 - system, 1 - user
 * \param s Pointer to the structure that receives SPI bus settings
 */
void spi_get_settings(uint8_t bus, spi_settings_t *s);

/**
 * \brief Set SPI bus mode
 * \param bus Bus ID: 0 - system, 1 - user
 * \param mode Bus mode
 */
void spi_set_mode(uint8_t bus, spi_mode_t mode);
/**
 * \brief Get mode of the SPI bus
 * \param bus Bus ID: 0 - system, 1 - user
 * \return Bus mode
 */
spi_mode_t spi_get_mode(uint8_t bus);

/**
 * \brief Set SPI bus frequency
 * Examples:
 *
 *     spi_set_frequency_div(1, SPI_FREQ_DIV_8M); // 8 MHz, predefined value
 *     ...
 *     spi_set_frequency_div(1, SPI_GET_FREQ_DIV(8, 10)); // divider = 8, count = 10,
 *                                                        // frequency = 80000000 Hz / 8 / 10 = 1000000 Hz
 *
 * \param bus Bus ID: 0 - system, 1 - user
 * \param divider Predivider of the system bus frequency (80MHz) in the 2 low
 *     bytes and period pulses count in the third byte. Please note that
 *     divider must be be in range 1..8192 and count in range 2..64. Use the
 *     macro SPI_GET_FREQ_DIV(divider, count) to get the correct parameter value.
 */
void spi_set_frequency_div(uint8_t bus, uint32_t divider);
/**
 * \brief Get SPI bus frequency as a divider
 * Example:
 *
 *   uint32_t old_freq = spi_get_frequency_div(1);
 *   spi_set_frequency_div(1, SPI_FREQ_DIV_8M);
 *   ...
 *   spi_set_frequency_div(1, old_freq);
 *
 * \param bus Bus ID: 0 - system, 1 - user
 * \return SPI frequency, as divider.
 */
inline uint32_t spi_get_frequency_div(uint8_t bus)
{
    return (FIELD2VAL(SPI_CLOCK_DIV_PRE, SPI(bus).CLOCK) + 1) |
           (FIELD2VAL(SPI_CLOCK_COUNT_NUM, SPI(bus).CLOCK) + 1);
}
/**
 * \brief Get SPI bus frequency in Hz
 * \param bus Bus ID: 0 - system, 1 - user
 * \return SPI frequency, Hz
 */
inline uint32_t spi_get_frequency_hz(uint8_t bus)
{
    return APB_CLK_FREQ /
           (FIELD2VAL(SPI_CLOCK_DIV_PRE, SPI(bus).CLOCK) + 1) /
           (FIELD2VAL(SPI_CLOCK_COUNT_NUM, SPI(bus).CLOCK) + 1);
}

/**
 * \brief Set SPI bus bit order
 * \param bus Bus ID: 0 - system, 1 - user
 * \param msb Bit order, MSB first if true
 */
void spi_set_msb(uint8_t bus, bool msb);
/**
 * \brief Get SPI bus bit order
 * \param bus Bus ID: 0 - system, 1 - user
 * \return msb Bit order, MSB first if true
 */
inline bool spi_get_msb(uint8_t bus)
{
    return !(SPI(bus).CTRL0 & (SPI_CTRL0_WR_BIT_ORDER | SPI_CTRL0_RD_BIT_ORDER));
}

/**
 * \brief Set SPI bus byte order
 * \param bus Bus ID: 0 - system, 1 - user
 * \param endianness Byte order
 */
void spi_set_endianness(uint8_t bus, spi_endianness_t endianness);
/**
 * \brief Get SPI bus byte order
 * \param bus Bus ID: 0 - system, 1 - user
 * \return endianness Byte order
 */
inline spi_endianness_t spi_get_endianness(uint8_t bus)
{
    return SPI(bus).USER0 & (SPI_USER0_WR_BYTE_ORDER | SPI_USER0_RD_BYTE_ORDER)
            ? SPI_BIG_ENDIAN
            : SPI_LITTLE_ENDIAN;
}

/**
 * \brief Transfer 8 bits over SPI
 * \param bus Bus ID: 0 - system, 1 - user
 * \param data Byte to send
 * \return Received byte
 */
uint8_t spi_transfer_8(uint8_t bus, uint8_t data);
/**
 * \brief Transfer 16 bits over SPI
 * \param bus Bus ID: 0 - system, 1 - user
 * \param data Word to send
 * \return Received word
 */
uint16_t spi_transfer_16(uint8_t bus, uint16_t data);
/**
 * \brief Transfer 32 bits over SPI
 * \param bus Bus ID: 0 - system, 1 - user
 * \param data dword to send
 * \return Received dword
 */
uint32_t spi_transfer_32(uint8_t bus, uint32_t data);
/**
 * \brief Transfer buffer of words over SPI
 * Please note that the buffer size is in words, not in bytes!
 * Example:
 *
 *    const uint16_t out_buf[] = { 0xa0b0, 0xa1b1, 0xa2b2, 0xa3b3 };
 *    uint16_t in_buf[sizeof(out_buf)];
 *    spi_init(1, SPI_MODE1, SPI_FREQ_DIV_4M, true, SPI_BIG_ENDIAN, true);
 *    spi_transfer(1, out_buf, in_buf, sizeof(out_buf), SPI_16BIT); // len = 4 words * 2 bytes = 8 bytes
 *
 * \param bus Bus ID: 0 - system, 1 - user
 * \param out_data Data to send.
 * \param in_data Receive buffer. If NULL, received data will be lost.
 * \param len Buffer size in words
 * \param word_size Size of the word
 * \return Transmitted/received words count
 */
size_t spi_transfer(uint8_t bus, const void *out_data, void *in_data, size_t len, spi_word_size_t word_size);

/**
 * \brief Add permanent command bits when transfert data over SPI
 * Example:
 *
 *    spi_set_command(1, 1, 0x01); // Set one command bit to 1
 *    for (uint8_t i = 0; i < x; i++ ) {
 *    	spi_transfer_8(1, 0x55);  // Send 1 bit command + 8 bits data x times
 *    }
 *    spi_clear_command(1); // Clear command
 *    spi_transfer_8(1, 0x55);  // Send 8 bits data
 *
 * \param bus Bus ID: 0 - system, 1 - user
 * \param bits Number of bits (max: 16).
 * \param data Command to send for each transfert.
 */
static inline void spi_set_command(uint8_t bus, uint8_t bits, uint16_t data)
{
    if (!bits) return;

    SPI(bus).USER0 |= SPI_USER0_COMMAND;                                //enable COMMAND function in SPI module
    uint16_t command;
    // Commands are always sent using little endian byte order
    if (!spi_get_msb(bus)) {
        // "data" are natively little endian, with LSB bit order
        // this makes all bits of the command ready to be sent as-is
        command = data;
    } else {
        // MSB
        command = data << (16 - bits);                                 //align command data to high bits
        command = ((command >> 8) & 0xff) | ((command << 8) & 0xff00); //swap byte order
    }
    SPI(bus).USER2 = SET_FIELD(SPI(bus).USER2, SPI_USER2_COMMAND_BITLEN, --bits);
    SPI(bus).USER2 = SET_FIELD(SPI(bus).USER2, SPI_USER2_COMMAND_VALUE, command);
}

/**
 * \brief Add permanent address bits when transfert data over SPI
 * Example:
 *
 *    spi_set_address(1,8,0x45); // Set one address byte to 0x45
 *    for (uint8_t i = 0 ; i < x ; i++ ) {
 *    	spi_transfer_16(1,0xC584);  // Send 16 bits address + 16 bits data x times
 *    }
 *    spi_clear_address(1); // Clear command
 *    spi_transfer_16(1,0x55);  // Send 16 bits data
 *
 * \param bus Bus ID: 0 - system, 1 - user
 * \param bits Number of bits (max: 32).
 * \param data Address to send for each transfert.
 */
static inline void spi_set_address(uint8_t bus, uint8_t bits, uint32_t data)
{
    if (!bits) return;

    SPI(bus).USER0 |= SPI_USER0_ADDR; //enable ADDRess function in SPI module
    // addresses are always sent using big endian byte order
    if (spi_get_msb(bus)) {
        SPI(bus).ADDR = data << (32 - bits); //align address data to high bits
    } else {
        // swap bytes from native little to command's big endian order
        // bits in each byte are already arranged properly for LSB
        SPI(bus).ADDR = (data & 0xff) << 24 | (data & 0xff00) << 8 | ((data >> 8) & 0xff00) | ((data >> 24) & 0xff);
    }
    SPI(bus).USER1 = SET_FIELD(SPI(bus).USER1, SPI_USER1_ADDR_BITLEN, --bits);
}

/**
 * \brief Add permanent dummy bits when transfert data over SPI
 * Example:
 *
 *    spi_set_dummy_bits(1, 4, false); // Set 4 dummy bit before Dout
 *    for (uint8_t i = 0; i < x; i++ ) {
 *    	spi_transfer_16(1, 0xC584);  // Send 4 bits dummy + 16 bits Dout x times
 *    }
 *    spi_set_dummy_bits(1, 4, true); // Set 4 dummy bit between Dout and Din
 *    spi_transfer_8(1, 0x55);  // Send 8 bits Dout + 4 bits dummy + 8 bits Din
 *
 * \param bus Bus ID: 0 - system, 1 - user
 * \param bits Number of bits
 * \param pos Position of dummy bit, between Dout and Din if true.
 */
static inline void spi_set_dummy_bits(uint8_t bus, uint8_t bits, bool pos)
{
    if (!bits) return;
    if (pos)
        SPI(bus).USER0 |= SPI_USER0_MISO; // Dummy bit will be between Dout and Din data if set
    SPI(bus).USER0 |= SPI_USER0_DUMMY; //enable dummy bits
    SPI(bus).USER1 = SET_FIELD(SPI(bus).USER1, SPI_USER1_DUMMY_CYCLELEN, --bits);
}

/**
 * \brief Clear adress Bits
 * \param bus Bus ID: 0 - system, 1 - user
 */
static inline void spi_clear_address(uint8_t bus)
{
    SPI(bus).USER0 &= ~(SPI_USER0_ADDR);
}

/**
 * \brief Clear command Bits
 * \param bus Bus ID: 0 - system, 1 - user
 */

static inline void spi_clear_command(uint8_t bus)
{
    SPI(bus).USER0 &= ~(SPI_USER0_COMMAND);
}

/**
 * \brief Clear dummy Bits
 * \param bus Bus ID: 0 - system, 1 - user
 */
static inline void spi_clear_dummy(uint8_t bus)
{
    SPI(bus).USER0 &= ~(SPI_USER0_DUMMY | SPI_USER0_MISO);
}

/**
 * \brief Send many 8 bits template over SPI
 * \param bus Bus ID: 0 - system, 1 - user
 * \param data Byte template (8 bits)
 * \param repeats Copy byte number
 */
void spi_repeat_send_8(uint8_t bus, uint8_t data, int32_t repeats);

/**
 * \brief Send many 16 bits template over SPI
 * \param bus Bus ID: 0 - system, 1 - user
 * \param data Word template (16 bits)
 * \param repeats Copy word number
 */
void spi_repeat_send_16(uint8_t bus, uint16_t data, int32_t repeats);

/**
 * \brief Send many 32 bits template over SPI
 * \param bus Bus ID: 0 - system, 1 - user
 * \param data Dualword template (32 bits)
 * \param repeats Copy dword number
 */
void spi_repeat_send_32(uint8_t bus, uint32_t data, int32_t repeats);

/**
 * \brief Repeatedly send byte over SPI and receive data
 * \param bus Bus ID: 0 - system, 1 - user
 * \param out_byte Byte to send
 * \param in_data Receive buffer
 * \param len Buffer size in words
 * \param word_size Size of the word
 */
void spi_read(uint8_t bus, uint8_t out_byte, void *in_data, size_t len, spi_word_size_t word_size);

#ifdef __cplusplus
}
#endif

#endif /* _ESP_SPI_H_ */
