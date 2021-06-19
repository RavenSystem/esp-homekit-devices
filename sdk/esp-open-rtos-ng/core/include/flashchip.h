/* flashchip.h
 *
 * sdk_flashchip_t structure used by the SDK and some bootrom routines
 *
 * This is in a separate include file because it's referenced by several other
 * headers which are otherwise independent of each other.
 *
 * Part of esp-open-rtos
 * Copyright (C) 2015 Alex Stewart and Angus Gratton
 * BSD Licensed as described in the file LICENSE
 */

#ifndef _FLASHCHIP_H
#define _FLASHCHIP_H

/* SDK/bootrom uses this structure internally to account for flash size.

   chip_size field is initialised during startup from the flash size
   saved in the image header (on the first 8 bytes of SPI flash).

   Other field are initialised to hardcoded values by the SDK.

   ** NOTE: This structure is passed to some bootrom routines and is therefore
   fixed.  Be very careful if you want to change it that you do not break
   things. **

   Based on RE work by @foogod at
   http://esp8266-re.foogod.com/wiki/Flashchip_%28IoT_RTOS_SDK_0.9.9%29
*/
typedef struct {
    uint32_t device_id;
    uint32_t chip_size;   /* in bytes */
    uint32_t block_size;  /* in bytes */
    uint32_t sector_size; /* in bytes */
    uint32_t page_size;   /* in bytes */
    uint32_t status_mask;
} sdk_flashchip_t;

extern sdk_flashchip_t sdk_flashchip;

#endif /* _FLASHCHIP_H */
