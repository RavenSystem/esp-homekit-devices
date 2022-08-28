/* Recreated Espressif libmain uart.o contents.

   Copyright (C) 2015 Espressif Systems. Derived from MIT Licensed SDK libraries.
   BSD Licensed as described in the file LICENSE
*/
#include "open_esplibs.h"
#if OPEN_LIBMAIN_UART
// The contents of this file are only built if OPEN_LIBMAIN_UART is set to true

#include "espressif/sdk_private.h"
#include "esp/uart_regs.h"

void sdk_uart_buff_switch(void) {
    /* No-Op */
}

void sdk_uart_div_modify(uint32_t uart_no, uint32_t new_divisor) {
    UART(uart_no).CLOCK_DIVIDER = new_divisor;
    UART(uart_no).CONF0 |= (UART_CONF0_TXFIFO_RESET | UART_CONF0_RXFIFO_RESET);
    UART(uart_no).CONF0 &= ~(UART_CONF0_TXFIFO_RESET | UART_CONF0_RXFIFO_RESET);
}

void sdk_Uart_Init(void) {
    /* No-Op */
}

#endif /* OPEN_LIBMAIN_UART */
