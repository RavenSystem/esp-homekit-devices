/*
 * Softuart for esp-open-rtos
 *
 * Copyright (C) 2017 Ruslan V. Uss <unclerus@gmail.com>
 * Copyright (C) 2016 Bernhard Guillon <Bernhard.Guillon@web.de>
 *
 * This code is based on Softuart from here [1] and reworked to
 * fit into esp-open-rtos. For now only the RX part is ported.
 * Also the configuration of the pin is for now hardcoded.
 *
 * it fits my needs to read the GY-GPS6MV2 module with 9600 8n1
 *
 * Original Copyright:
 * Copyright (c) 2015 plieningerweb
 *
 * MIT Licensed as described in the file LICENSE
 *
 * 1 https://github.com/plieningerweb/esp8266-software-uart
 */
#ifndef SOFTUART_H_
#define SOFTUART_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef SOFTUART_MAX_UARTS
    #define SOFTUART_MAX_UARTS 2
#endif

#ifndef SOFTUART_MAX_RX_BUFF
    #define SOFTUART_MAX_RX_BUFF 64 //!< Must be power of two: 2, 4, 8, 16 etc.
#endif

/**
 * Initialize software uart and setup interrupt handler
 * @param uart_no Software uart index, 0..SOFTUART_MAX_UARTS
 * @param baudrate Baudrate, e.g. 9600, 19200, etc
 * @param rx_pin GPIO pin number for RX
 * @param tx_pin GPIO pin number for TX
 * @return true if no errors occured otherwise false
 */
bool softuart_open(uint8_t uart_no, uint32_t baudrate, uint8_t rx_pin, uint8_t tx_pin);

/**
 * Deinitialize software uart
 * @param uart_no Software uart index, 0..SOFTUART_MAX_UARTS
 * @return true if no errors occured otherwise false
 */
bool softuart_close(uint8_t uart_no);

/**
 * Put char to software uart
 * @param uart_no Software uart index, 0..SOFTUART_MAX_UARTS
 * @param c Char
 * @return true if no errors occured otherwise false
 */
bool softuart_put(uint8_t uart_no, char c);

/**
 * Put string to software uart
 * @param uart_no Software uart index, 0..SOFTUART_MAX_UARTS
 * @param s Null-terminated string
 * @return true if no errors occured otherwise false
 */
bool softuart_puts(uint8_t uart_no, const char *s);

/**
 * Check if data is available
 * @param uart_no Software uart index, 0..SOFTUART_MAX_UARTS
 * @return true if data is available otherwise false
 */
bool softuart_available(uint8_t uart_no);

/**
 * Read current byte from internal buffer if available.
 *
 * NOTE: This call is non blocking.
 * NOTE: You have to check softuart_available() first.
 * @param uart_no Software uart index, 0..SOFTUART_MAX_UARTS
 * @return current byte if available otherwise 0
 */
uint8_t softuart_read(uint8_t uart_no);

#ifdef __cplusplus
}
#endif

#endif /* SOFTUART_H_ */
