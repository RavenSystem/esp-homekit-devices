/* "Boot ROM" function signatures

   Note that a lot of the ROM functions used in the IoT SDK aren't
   referenced from the Espressif RTOS SDK, and are probably incompatible.
 */
#ifndef _ESP_ROM_H
#define _ESP_ROM_H

#include "esp/types.h"
#include "flashchip.h"

#ifdef	__cplusplus
extern "C" {
#endif

void Cache_Read_Disable(void);

/* http://esp8266-re.foogod.com/wiki/Cache_Read_Enable

   Note: when compiling non-OTA we use the ROM version of this
   function, but for OTA we use the version in extras/rboot-ota that
   maps the correct flash page for OTA support.
 */
void Cache_Read_Enable(uint32_t odd_even, uint32_t mb_count, uint32_t no_idea);

/* Low-level SPI flash read/write routines */
int Enable_QMode(sdk_flashchip_t *chip);
int Disable_QMode(sdk_flashchip_t *chip);
int SPI_page_program(sdk_flashchip_t *chip, uint32_t dest_addr, uint32_t *src_addr, uint32_t size);
int SPI_read_data(sdk_flashchip_t *chip, uint32_t src_addr, uint32_t *dest_addr, uint32_t size);
int SPI_write_enable(sdk_flashchip_t *chip);
int SPI_sector_erase(sdk_flashchip_t *chip, uint32_t addr);
int SPI_read_status(sdk_flashchip_t *chip, uint32_t *status);
int SPI_write_status(sdk_flashchip_t *chip, uint32_t status);
int Wait_SPI_Idle(sdk_flashchip_t *chip);

#ifdef	__cplusplus
}
#endif

#endif /* _ESP_ROM_H */
