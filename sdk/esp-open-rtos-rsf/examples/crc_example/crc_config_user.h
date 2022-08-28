/*
 * perso_config.h
 *
 *  Created on: 11 févr. 2017
 *      Author: lilian
 */

#ifndef CRC_CONFIG_USER_H_
#define CRC_CONFIG_USER_H_

#include "espressif/esp_common.h"
#include "FreeRTOS.h"

#define CRC_DEBUG 0
#define CRC_1BYTE_SUPPORT 1
#define CRC_4BYTE_SUPPORT 0
#define CRC_8BYTE_SUPPORT 0

typedef uint8_t     crc_8;
typedef uint16_t    crc_16;
typedef uint32_t    crc_32;
typedef uint64_t    crc_64;

#endif /* CRC_CONFIG_USER_H_ */

