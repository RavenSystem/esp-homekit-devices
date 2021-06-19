/** esp/phy.h
 *
 * PHY hardware management functions.
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 ChefSteps, Inc
 * BSD Licensed as described in the file LICENSE
 */
#include <esp/phy.h>
#include <esp/gpio.h>

void bt_coexist_configure(bt_coexist_mode_t mode, uint8_t bt_active_pin, uint8_t bt_priority_pin)
{
    /* Disable coexistence entirely before changing pin assignments */
    GPIO.OUT &= ~(GPIO_OUT_BT_COEXIST_MASK);
    uint32_t new_mask = 0;

    new_mask = VAL2FIELD_M(GPIO_OUT_BT_ACTIVE_PIN, bt_active_pin) +
        VAL2FIELD_M(GPIO_OUT_BT_PRIORITY_PIN, bt_priority_pin);

    if(mode == BT_COEXIST_USE_BT_ACTIVE || mode == BT_COEXIST_USE_BT_ACTIVE_PRIORITY) {
        gpio_enable(bt_active_pin, GPIO_INPUT);
        new_mask |= GPIO_OUT_BT_ACTIVE_ENABLE;
    }
    if(mode == BT_COEXIST_USE_BT_ACTIVE_PRIORITY) {
        gpio_enable(bt_priority_pin, GPIO_INPUT);
        new_mask |= GPIO_OUT_BT_PRIORITY_ENABLE;
    }
    GPIO.OUT |= new_mask;
}


