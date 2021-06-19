#ifndef DRIVER_DS18B20_H_
#define DRIVER_DS18B20_H_

#include "onewire/onewire.h"

#ifdef	__cplusplus
extern "C" {
#endif

/** @file ds18b20.h
 *
 *  Communicate with the DS18B20 family of one-wire temperature sensor ICs.
 *
 */

typedef onewire_addr_t ds18b20_addr_t;

/** An address value which can be used to indicate "any device on the bus" */
#define DS18B20_ANY ONEWIRE_NONE

/** Find the addresses of all DS18B20 devices on the bus.
 *
 *  Scans the bus for all devices and places their addresses in the supplied
 *  array.  If there are more than `addr_count` devices on the bus, only the
 *  first `addr_count` are recorded.
 *
 *  @param pin         The GPIO pin connected to the DS18B20 bus
 *  @param addr_list   A pointer to an array of ds18b20_addr_t values.  This
 *                     will be populated with the addresses of the found
 *                     devices.
 *  @param addr_count  Number of slots in the `addr_list` array.  At most this
 *                     many addresses will be returned.
 *
 *  @returns The number of devices found.  Note that this may be less than,
 *  equal to, or more than `addr_count`, depending on how many DS18B20 devices
 *  are attached to the bus.
 */
int ds18b20_scan_devices(int pin, ds18b20_addr_t *addr_list, int addr_count);

/** Tell one or more sensors to perform a temperature measurement and
 *  conversion (CONVERT_T) operation.  This operation can take up to 750ms to
 *  complete.
 *
 *  If `wait=true`, this routine will automatically drive the pin high for the
 *  necessary 750ms after issuing the command to ensure parasitically-powered
 *  devices have enough power to perform the conversion operation (for
 *  non-parasitically-powered devices, this is not necessary but does not
 *  hurt).  If `wait=false`, this routine will drive the pin high, but will
 *  then return immediately.  It is up to the caller to wait the requisite time
 *  and then depower the bus using onewire_depower() or by issuing another
 *  command once conversion is done.
 *
 *  @param pin   The GPIO pin connected to the DS18B20 device
 *  @param addr  The 64-bit address of the device on the bus.  This can be set
 *               to ::DS18B20_ANY to send the command to all devices on the bus
 *               at the same time.
 *  @param wait  Whether to wait for the necessary 750ms for the DS18B20 to
 *               finish performing the conversion before returning to the
 *               caller (You will normally want to do this).
 *
 *  @returns `true` if the command was successfully issued, or `false` on error.
 */
bool ds18b20_measure(int pin, ds18b20_addr_t addr, bool wait);

/** Read the value from the last CONVERT_T operation.
 *
 *  This should be called after ds18b20_measure() to fetch the result of the
 *  temperature measurement.
 *
 *  @param pin     The GPIO pin connected to the DS18B20 device
 *  @param addr    The 64-bit address of the device to read.  This can be set
 *                 to ::DS18B20_ANY to read any device on the bus (but note
 *                 that this will only work if there is exactly one device
 *                 connected, or they will corrupt each others' transmissions)
 *
 *  @returns The temperature in degrees Celsius, or NaN if there was an error.
 */
float ds18b20_read_temperature(int pin, ds18b20_addr_t addr);

/** Read the value from the last CONVERT_T operation for multiple devices.
 *
 *  This should be called after ds18b20_measure() to fetch the result of the
 *  temperature measurement.
 *
 *  @param pin         The GPIO pin connected to the DS18B20 bus
 *  @param addr_list   A list of addresses for devices to read.
 *  @param addr_count  The number of entries in `addr_list`.
 *  @param result_list An array of floats to hold the returned temperature
 *                     values.  It should have at least `addr_count` entries.
 *
 *  @returns `true` if all temperatures were fetched successfully, or `false`
 *  if one or more had errors (the temperature for erroring devices will be
 *  returned as NaN).
 */
bool ds18b20_read_temp_multi(int pin, ds18b20_addr_t *addr_list, int addr_count, float *result_list);

/** Perform a ds18b20_measure() followed by ds18b20_read_temperature()
 *
 *  @param pin     The GPIO pin connected to the DS18B20 device
 *  @param addr    The 64-bit address of the device to read.  This can be set
 *                 to ::DS18B20_ANY to read any device on the bus (but note
 *                 that this will only work if there is exactly one device
 *                 connected, or they will corrupt each others' transmissions)
 *
 *  @returns The temperature in degrees Celsius, or NaN if there was an error.
 */
float ds18b20_measure_and_read(int pin, ds18b20_addr_t addr);

/** Perform a ds18b20_measure() followed by ds18b20_read_temp_multi()
 *
 *  @param pin         The GPIO pin connected to the DS18B20 bus
 *  @param addr_list   A list of addresses for devices to read.
 *  @param addr_count  The number of entries in `addr_list`.
 *  @param result_list An array of floats to hold the returned temperature
 *                     values.  It should have at least `addr_count` entries.
 *
 *  @returns `true` if all temperatures were fetched successfully, or `false`
 *  if one or more had errors (the temperature for erroring devices will be
 *  returned as NaN).
 */
bool ds18b20_measure_and_read_multi(int pin, ds18b20_addr_t *addr_list, int addr_count, float *result_list);

/** Read the scratchpad data for a particular DS18B20 device.
 *
 *  This is not generally necessary to do directly.  It is done automatically
 *  as part of ds18b20_read_temperature().
 *
 *  @param pin     The GPIO pin connected to the DS18B20 device
 *  @param addr    The 64-bit address of the device to read.  This can be set
 *                 to ::DS18B20_ANY to read any device on the bus (but note
 *                 that this will only work if there is exactly one device
 *                 connected, or they will corrupt each others' transmissions)
 *  @param buffer  An 8-byte buffer to hold the read data.
 *
 *  @returns `true` if the data was read successfully, or `false` on error.
 */
bool ds18b20_read_scratchpad(int pin, ds18b20_addr_t addr, uint8_t *buffer);

// The following are obsolete/deprecated APIs

typedef struct {
    uint8_t id;
    float value;
} ds_sensor_t;

// Scan all ds18b20 sensors on bus and return its amount.
// Result are saved in array of ds_sensor_t structure.
uint8_t ds18b20_read_all(uint8_t pin, ds_sensor_t *result);

// This method is just to demonstrate how to read 
// temperature from single dallas chip.
float ds18b20_read_single(uint8_t pin);

#ifdef	__cplusplus
}
#endif

#endif  /* DRIVER_DS18B20_H_ */
