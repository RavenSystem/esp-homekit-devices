/* sdk_private.h

   This source file contains function prototypes for "private" but
   useful functions defined in the "binary blob" ESP IoT RTOS SDK libraries.

   For the "public" API, check the esp_common header file and the various
   sub-headers it includes.

   Function names here have the 'sdk_' prefix that is attached to all
   binary library symbols by the esp-open-rtos build process.

   This file is a part of esp-open-rtos.
*/
#ifndef SDK_PRIVATE_H
#define SDK_PRIVATE_H

#include <stdint.h>

#ifdef	__cplusplus
extern "C" {
#endif

struct ip4_addr;

/*********************************************
* Defined in libmain.a
*********************************************
*/

/* Change UART divider without re-initialising UART.

   uart_no = 0 or 1 for which UART
   new_divisor = Calculated in the form UART_CLK_FREQ / BAUD
 */
void sdk_uart_div_modify(uint32_t uart_no, uint32_t new_divisor);

/* Read a single character from the UART.
   Returns 0 on success, 1 if no character in fifo
 */
int sdk_uart_rx_one_char(char *buf);

/* Write a single character to the UART.
 */
void sdk_os_putc(char c);

#ifdef	__cplusplus
}
#endif

#endif
