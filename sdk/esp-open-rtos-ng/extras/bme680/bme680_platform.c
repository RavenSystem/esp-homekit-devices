/*
 * Driver for Bosch Sensortec BME680 digital temperature, humidity, pressure
 * and gas sensor connected to I2C or SPI
 *
 * This driver is for the usage with the ESP8266 and FreeRTOS (esp-open-rtos)
 * [https://github.com/SuperHouse/esp-open-rtos]. It is also working with ESP32
 * and ESP-IDF [https://github.com/espressif/esp-idf.git] as well as Linux
 * based systems using a wrapper library for ESP8266 functions.
 *
 * ---------------------------------------------------------------------------
 *
 * The BSD License (3-clause license)
 *
 * Copyright (c) 2017 Gunar Schorcht (https://github.com/gschorcht)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * Platform file: platform specific definitions, includes and functions
 */

#include "bme680_platform.h"

// platform specific SPI functions

static const spi_settings_t bus_settings = {
    .mode         = SPI_MODE0,
    .freq_divider = SPI_FREQ_DIV_1M,
    .msb          = true,
    .minimal_pins = false,
    .endianness   = SPI_LITTLE_ENDIAN
};

bool spi_device_init (uint8_t bus, uint8_t cs)
{
    gpio_enable(cs, GPIO_OUTPUT);
    gpio_write (cs, true);
    return true;
}

size_t spi_transfer_pf(uint8_t bus, uint8_t cs, const uint8_t *mosi, uint8_t *miso, uint16_t len)
{
    spi_settings_t old_settings;

    spi_get_settings(bus, &old_settings);
    spi_set_settings(bus, &bus_settings);
    gpio_write(cs, false);

    size_t transfered = spi_transfer (bus, (const void*)mosi, (void*)miso, len, SPI_8BIT);

    gpio_write(cs, true);
    spi_set_settings(bus, &old_settings);
    
    return transfered;
}

