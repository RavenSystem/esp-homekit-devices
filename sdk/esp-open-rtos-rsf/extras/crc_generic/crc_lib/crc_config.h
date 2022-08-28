/**
 * CRC GENERIC LIBRARY.
 *
 * Copyright (c) 2016 Zaltora (https://github.com/Zaltora)
 *
 * BSD Licensed as described in the file LICENSE
 */

#ifndef CRC_CONFIG_H_
#define CRC_CONFIG_H_

#include "crc_config_user.h"

#ifndef CRC_DEBUG
#define CRC_DEBUG 0
#endif

#ifndef CRC_1BYTE_SUPPORT
#define CRC_1BYTE_SUPPORT 1
#endif
#ifndef CRC_2BYTE_SUPPORT
#define CRC_2BYTE_SUPPORT 1
#endif
#ifndef CRC_4BYTE_SUPPORT
#define CRC_4BYTE_SUPPORT 1
#endif
#ifndef CRC_8BYTE_SUPPORT
#define CRC_8BYTE_SUPPORT 1
#endif

#ifndef NULL
#define NULL (void *)0
#endif

#endif /* CRC_CONFIG_H_ */
