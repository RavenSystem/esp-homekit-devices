/* ESP8266 Xtensa interrupt management functions
 *
 *
 * Part of esp-open-rtos
 * Copyright (C) 2015 Angus Gratton
 * BSD Licensed as described in the file LICENSE
 */
#include <esp/interrupts.h>

typedef struct _xt_isr_entry_ {
    _xt_isr handler;
    void *arg;
} _xt_isr_entry;

_xt_isr_entry isr[16];

bool esp_in_isr;

void IRAM _xt_isr_attach(uint8_t i, _xt_isr func, void *arg)
{
    isr[i].handler = func;
    isr[i].arg = arg;
}

/* Generic ISR handler.

   Handles all flags set for interrupts in 'intset'.
*/
uint16_t IRAM _xt_isr_handler(uint16_t intset)
{
    esp_in_isr = true;

    /* WDT has highest priority (occasional WDT resets otherwise) */
    if (intset & BIT(INUM_WDT)) {
        _xt_clear_ints(BIT(INUM_WDT));
        isr[INUM_WDT].handler(NULL);
        intset -= BIT(INUM_WDT);
    }

    while (intset) {
        uint8_t index = __builtin_ffs(intset) - 1;
        uint16_t mask = BIT(index);
        _xt_clear_ints(mask);
        _xt_isr handler = isr[index].handler;
        if (handler) {
            handler(isr[index].arg);
        }
        intset -= mask;
    }

    esp_in_isr = false;

    return 0;
}
