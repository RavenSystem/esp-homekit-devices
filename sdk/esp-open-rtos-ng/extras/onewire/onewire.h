#ifndef __ONEWIRE_H__
#define __ONEWIRE_H__

#include <espressif/esp_misc.h> // sdk_os_delay_us
#include "FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @file onewire.h
 *
 *  Routines to access devices using the Dallas Semiconductor 1-Wire(tm)
 *  protocol.
 */

/** Select the table-lookup method of computing the 8-bit CRC
 *  by setting this to 1 during compilation.  The lookup table enlarges code
 *  size by about 250 bytes.  By default, a slower but very compact algorithm
 *  is used.
 */
#ifndef ONEWIRE_CRC8_TABLE
#define ONEWIRE_CRC8_TABLE 0
#endif

/** Type used to hold all 1-Wire device ROM addresses (64-bit) */
typedef uint64_t onewire_addr_t;

/** Structure to contain the current state for onewire_search_next(), etc */
typedef struct {
    uint8_t rom_no[8];
    uint8_t last_discrepancy;
    bool last_device_found;
} onewire_search_t;

/** ::ONEWIRE_NONE is an invalid ROM address that will never occur in a device
 *  (CRC mismatch), and so can be useful as an indicator for "no-such-device",
 *   etc.
 */
#define ONEWIRE_NONE ((onewire_addr_t)(0xffffffffffffffffLL))

/** Perform a 1-Wire reset cycle.
 *
 *  @param pin  The GPIO pin connected to the 1-Wire bus.
 *
 *  @returns `true` if at least one device responds with a presence pulse,
 *           `false` if no devices were detected (or the bus is shorted, etc)
 */
bool onewire_reset(int pin);

/** Issue a 1-Wire rom select command to select a particular device.
 *
 *  It is necessary to call onewire_reset() before calling this function.
 *
 *  @param pin   The GPIO pin connected to the 1-Wire bus.
 *  @param addr  The ROM address of the device to select
 *
 *  @returns `true` if the "ROM select" command could be succesfully issued,
 *           `false` if there was an error.
 */
bool onewire_select(int pin, const onewire_addr_t addr);

/** Issue a 1-Wire "skip ROM" command to select *all* devices on the bus.
 *
 *  It is necessary to call onewire_reset() before calling this function.
 *
 *  @param pin   The GPIO pin connected to the 1-Wire bus.
 *
 *  @returns `true` if the "skip ROM" command could be succesfully issued,
 *           `false` if there was an error.
 */
bool onewire_skip_rom(int pin);

/** Write a byte on the onewire bus.
 *
 *  The writing code uses open-drain mode and expects the pullup resistor to
 *  pull the line high when not driven low.  If you need strong power after the
 *  write (e.g. DS18B20 in parasite power mode) then call onewire_power() after
 *  this is complete to actively drive the line high.
 *
 *  @param pin   The GPIO pin connected to the 1-Wire bus.
 *  @param v     The byte value to write
 *
 *  @returns `true` if successful, `false` on error.
 */
bool onewire_write(int pin, uint8_t v);

/** Write multiple bytes on the 1-Wire bus.
 *
 *  See onewire_write() for more info.
 *
 *  @param pin    The GPIO pin connected to the 1-Wire bus.
 *  @param buf    A pointer to the buffer of bytes to be written
 *  @param count  Number of bytes to write
 *
 *  @returns `true` if all bytes written successfully, `false` on error.
 */
bool onewire_write_bytes(int pin, const uint8_t *buf, size_t count);

/** Read a byte from a 1-Wire device.
 *
 *  @param pin    The GPIO pin connected to the 1-Wire bus.
 *
 *  @returns the read byte on success, negative value on error.
 */
int onewire_read(int pin);

/** Read multiple bytes from a 1-Wire device.
 *
 *  @param pin    The GPIO pin connected to the 1-Wire bus.
 *  @param buf    A pointer to the buffer to contain the read bytes
 *  @param count  Number of bytes to read
 *
 *  @returns `true` on success, `false` on error.
 */
bool onewire_read_bytes(int pin, uint8_t *buf, size_t count);

/** Actively drive the bus high to provide extra power for certain operations
 *  of parasitically-powered devices.
 *
 *  For parasitically-powered devices which need more power than can be
 *  provided via the normal pull-up resistor, it may be necessary for some
 *  operations to drive the bus actively high.  This function can be used to
 *  perform that operation.
 *
 *  The bus can be depowered once it is no longer needed by calling
 *  onewire_depower(), or it will be depowered automatically the next time
 *  onewire_reset() is called to start another command.
 *
 *  Note: Make sure the device(s) you are powering will not pull more current
 *  than the ESP8266 is able to supply via its GPIO pins (this is especially
 *  important when multiple devices are on the same bus and they are all
 *  performing a power-intensive operation at the same time (i.e. multiple
 *  DS18B20 sensors, which have all been given a "convert T" operation by using
 *  onewire_skip_rom())).
 *
 *  Note: This routine will check to make sure that the bus is already high
 *  before driving it, to make sure it doesn't attempt to drive it high while
 *  something else is pulling it low (which could cause a reset or damage the
 *  ESP8266).
 *
 *  @param pin    The GPIO pin connected to the 1-Wire bus.
 *
 *  @returns `true` on success, `false` on error.
 */
bool onewire_power(int pin);

/** Stop forcing power onto the bus.
 *
 *  You only need to do this if you previously called onewire_power() to drive
 *  the bus high and now want to allow it to float instead.  Note that
 *  onewire_reset() will also automatically depower the bus first, so you do
 *  not need to call this first if you just want to start a new operation.
 *
 *  @param pin    The GPIO pin connected to the 1-Wire bus.
 */
void onewire_depower(int pin);

/** Clear the search state so that it will start from the beginning on the next
 *  call to onewire_search_next().
 *
 *  @param search  The onewire_search_t structure to reset.
 */
void onewire_search_start(onewire_search_t *search);

/** Setup the search to search for devices with the specified "family code".
 *
 *  @param search       The onewire_search_t structure to update.
 *  @param family_code  The "family code" to search for.
 */
void onewire_search_prefix(onewire_search_t *search, uint8_t family_code);

/** Search for the next device on the bus.
 *
 *  The order of returned device addresses is deterministic. You will always
 *  get the same devices in the same order.
 *
 *  @returns the address of the next device on the bus, or ::ONEWIRE_NONE if
 *  there is no next address.  ::ONEWIRE_NONE might also mean that the bus is
 *  shorted, there are no devices, or you have already retrieved all of them.
 *
 *  It might be a good idea to check the CRC to make sure you didn't get
 *  garbage.
 */
onewire_addr_t onewire_search_next(onewire_search_t *search, int pin);

/** Compute a Dallas Semiconductor 8 bit CRC.
 *
 *  These are used in the ROM address and scratchpad registers to verify the
 *  transmitted data is correct.
 */
uint8_t onewire_crc8(const uint8_t *data, uint8_t len);

/** Compute the 1-Wire CRC16 and compare it against the received CRC.
 *
 *  Example usage (reading a DS2408):
 *  @code
 *      // Put everything in a buffer so we can compute the CRC easily.
 *      uint8_t buf[13];
 *      buf[0] = 0xF0;    // Read PIO Registers
 *      buf[1] = 0x88;    // LSB address
 *      buf[2] = 0x00;    // MSB address
 *      onewire_write_bytes(pin, buf, 3);    // Write 3 cmd bytes
 *      onewire_read_bytes(pin, buf+3, 10);  // Read 6 data bytes, 2 0xFF, 2 CRC16
 *      if (!onewire_check_crc16(buf, 11, &buf[11])) {
 *          // TODO: Handle error.
 *      }     
 *  @endcode
 *           
 *  @param input         Array of bytes to checksum.
 *  @param len           Number of bytes in `input`
 *  @param inverted_crc  The two CRC16 bytes in the received data.
 *                       This should just point into the received data,
 *                       *not* at a 16-bit integer.
 *  @param crc_iv        The crc starting value (optional)
 *
 *  @returns `true` if the CRC matches, `false` otherwise.
 */
bool onewire_check_crc16(const uint8_t* input, size_t len, const uint8_t* inverted_crc, uint16_t crc_iv);

/** Compute a Dallas Semiconductor 16 bit CRC.
 *
 *  This is required to check the integrity of data received from many 1-Wire
 *  devices.  Note that the CRC computed here is *not* what you'll get from the
 *  1-Wire network, for two reasons:
 *    1. The CRC is transmitted bitwise inverted.
 *    2. Depending on the endian-ness of your processor, the binary
 *       representation of the two-byte return value may have a different
 *       byte order than the two bytes you get from 1-Wire.
 *
 *  @param input   Array of bytes to checksum.
 *  @param len     How many bytes are in `input`.
 *  @param crc_iv  The crc starting value (optional)
 *
 *  @returns the CRC16, as defined by Dallas Semiconductor.
 */
uint16_t onewire_crc16(const uint8_t* input, size_t len, uint16_t crc_iv);

#ifdef __cplusplus
}
#endif

#endif  /* __ONEWIRE_H__ */
