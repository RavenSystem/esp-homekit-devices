//////////////////////////////////////////////////
// rBoot open source boot loader for ESP8266.
// Copyright 2015 Richard A Burton
// richardaburton@gmail.com
// See license.txt for license terms.
//////////////////////////////////////////////////

#include <rboot.h>

#ifdef BOOT_BIG_FLASH

// plain sdk defaults to iram
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern void Cache_Read_Disable(void);
extern uint32 SPIRead(uint32, void*, uint32);
extern void ets_printf(const char*, ...);
extern void Cache_Read_Enable(uint32, uint32, uint32);

uint8 rBoot_mmap_1 = 0xff;
uint8 rBoot_mmap_2 = 0xff;

// this function must remain in iram
void IRAM_ATTR Cache_Read_Enable_New(void) {
	
	if (rBoot_mmap_1 == 0xff) {
		uint32 val;
		rboot_config conf;

		SPIRead(BOOT_CONFIG_SECTOR * SECTOR_SIZE, &conf, sizeof(rboot_config));

#ifdef BOOT_RTC_ENABLED
		// rtc support here isn't written ideally, we don't read the whole structure and
		// we don't check the checksum. However this code is only run on first boot, so
		// the rtc data should have just been set and the user app won't have had chance
		// to corrupt it or suspend or anything else that might upset it. And if
		// it were bad what should we do anyway? We can't just ignore bad data here, we
		// need it. But the main reason is that this code must be in iram, which is in
		// very short supply, doing this "properly" increases the size about 3x

		// used only to calculate offset into structure, should get optimized out
		rboot_rtc_data rtc;
		uint8 off = (uint8*)&rtc.last_rom - (uint8*)&rtc;
		// get the four bytes containing the one of interest
		volatile uint32 *rtcd = (uint32*)(0x60001100 + (RBOOT_RTC_ADDR*4) + (off & ~3));
		val = *rtcd;
		// extract the one of interest
		val = ((uint8*)&val)[off & 3];
		// get address of rom
		val = conf.roms[val];
#else
		val = conf.roms[conf.current_rom];
#endif

		val /= 0x100000;

		rBoot_mmap_2 = val / 2;
		rBoot_mmap_1 = val % 2;
		
		//ets_printf("mmap %d,%d,1\r\n", rBoot_mmap_1, rBoot_mmap_2);
	}
	
	Cache_Read_Enable(rBoot_mmap_1, rBoot_mmap_2, 1);
}

#ifdef __cplusplus
}
#endif

#endif

