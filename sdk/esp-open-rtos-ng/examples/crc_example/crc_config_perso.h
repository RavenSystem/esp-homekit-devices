/*
 * perso_config.h
 *
 *  Created on: 11 févr. 2017
 *      Author: lilian
 */

#include "espressif/esp_common.h"
#include "FreeRTOS.h"

#define CRC_DEBUG 0
#define CRC_4BYTE_SUPPORT 0
/* Use the defaults for everything else */
#include_next "crc_config.h"

