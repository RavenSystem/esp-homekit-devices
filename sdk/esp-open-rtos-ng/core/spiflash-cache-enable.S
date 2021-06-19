/* OTA-compatible Cache_Read_Enable function
 *
 * This gets called from the SDK when it wants to enable the flash mapping.
 *
 * In recent SDK versions it's been replaced with a Cache_Read_Enable_New
 * function that takes note of OTA stuff.
 *
 * For esp-open-rtos we just replace the ROM function with this wrapper.
 *
 *
 * Part of esp-open-rtos
 * Copyright (C) 2015 Superhouse Automation Pty Ltd
 * BSD Licensed as described in the file LICENSE
*/
#define RBOOT_CONFIG_BASE (0x40200000 + 0x1000)
#define RBOOT_ROMS_OFFS 0x8 /* offset of rboot_config_t.roms array in config */

#define RBOOT_MEGABYTE_DEFAULT 0x80

	.section .data
	.global rboot_megabyte
rboot_megabyte:
	.byte RBOOT_MEGABYTE_DEFAULT

	.section .data
	.local cache_return_save
	.align 4
cache_return_save:
	.word 0

	.text
	.section .iram1.text, "x"
	.literal_position
        .align  4
        .global Cache_Read_Enable
        .type   Cache_Read_Enable, @function /* it's not really a function, but treat it like one */
Cache_Read_Enable:
	movi a2, cache_return_save
	s32i a0, a2, 0 /* save a0 here */
	movi a2, rboot_megabyte
	l8ui a2, a2, 0
	bnei a2, RBOOT_MEGABYTE_DEFAULT, .Lalready_initialised

	/* map the first megabyte of flash */
	movi a2, 0
	movi a3, 0
	movi a4, 1
	call0 rom_Cache_Read_Enable

	movi a3, RBOOT_CONFIG_BASE
	l32i a2, a3, 0 	      /* 32-bit flash read */
	extui a2, a2, 24, 8   /* 3rd byte is 'current' field of config */
	slli a2, a2, 2        /* Left shift by two, becomes offset into rom array */
	add a4, a2, a3	      /* Add the base config address */
	l32i a4, a4, RBOOT_ROMS_OFFS        /* Read from the ROM array */
	extui a4, a4, 20, 8   /* now a4 is number of megabytes */

	/* save to rboot_megabyte */
	movi a3, rboot_megabyte
	s8i a4, a3, 0

	/* re-disable cache? */
	call0 Cache_Read_Disable

.Lalready_initialised:
	movi a4, rboot_megabyte
	l32i a4, a4, 0
	extui a2, a4, 0, 1 /* a2 is now lsb of a4 (odd/even) */
	srli a3, a4, 1 	   /* a3 is half value of mb */
	movi a4, 1
	call0 rom_Cache_Read_Enable
	movi a0, cache_return_save /* restore a0 return address */
	l32i a0, a0, 0
	ret.n
