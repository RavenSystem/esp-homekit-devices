/** esp/phy.h
 *
 * PHY hardware management functions.
 *
 * These are not enough to configure the ESP8266 PHY by themselves
 * (yet), but they provide utilities to modify the configuration set
 * up via the SDK.
 *
 * Functions implemented here deal directly with the hardware, not the
 * SDK software layers.
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 ChefSteps, Inc
 * BSD Licensed as described in the file LICENSE
 */
#ifndef _ESP_PHY_H
#define _ESP_PHY_H

#include <esp/types.h>
#include <common_macros.h>

#include <stdint.h>

typedef enum {
    BT_COEXIST_NONE,
    BT_COEXIST_USE_BT_ACTIVE,
    BT_COEXIST_USE_BT_ACTIVE_PRIORITY,
} bt_coexist_mode_t;

/** Override the Bluetooth Coexistence BT_ACTIVE pin setting
    taken from the phy_info structure.

    This enables other pins to be used for Bluetooth Coexistence
    signals (rather than just the two provided for by phy_info). (Note
    that not that not all pins are confirmed to work, GPIO 0 is
    confirmed not usable as the SDK configures it as the WLAN_ACTIVE
    output - even if you change the pin mode the SDK will change it
    back.)

    To change pins and have coexistence work successfully the BT
    coexistence feature must be enabled in the phy_info configuration
    (get_default_phy_info()). You can then modify the initial
    configuration by calling this function from your own user_init
    function.

    (Eventually we should be able to support enough PHY registers
    to enable coexistence without SDK support at all, but not yet.)

    This function will enable bt_active_pin & bt_priority_as GPIO
    inputs, according to the mode parameter.

    Remember that the default Bluetooth Coexistence pins will be
    configured as GPIOs by the SDK already, so you may want to
    reconfigure/re-iomux them after calling this function.
*/
void bt_coexist_configure(bt_coexist_mode_t mode, uint8_t bt_active_pin, uint8_t bt_priority_pin);

#endif
