//////////////////////////////////////////////////
// rBoot open source boot loader for ESP8266.
// Copyright 2015 Richard A Burton
// richardaburton@gmail.com
// See license.txt for license terms.
//////////////////////////////////////////////////

#include "rboot-private.h"
#include <rboot-hex2a.h>

#ifndef UART_CLK_FREQ
// reset apb freq = 2x crystal freq: http://esp8266-re.foogod.com/wiki/Serial_UART
#define UART_CLK_FREQ	(26000000 * 2)
#endif

static uint32 check_image(uint32 readpos) {

	uint8 buffer[BUFFER_SIZE];
	uint8 sectcount;
	uint8 sectcurrent;
	uint8 chksum = CHKSUM_INIT;
	uint32 loop;
	uint32 remaining;
	uint32 romaddr;

	rom_header_new *header = (rom_header_new*)buffer;
	section_header *section = (section_header*)buffer;

	if (readpos == 0 || readpos == 0xffffffff) {
		return 0;
	}

	// read rom header
	if (SPIRead(readpos, header, sizeof(rom_header_new)) != 0) {
		return 0;
	}

	// check header type
	if (header->magic == ROM_MAGIC) {
		// old type, no extra header or irom section to skip over
		romaddr = readpos;
		readpos += sizeof(rom_header);
		sectcount = header->count;
	} else if (header->magic == ROM_MAGIC_NEW1 && header->count == ROM_MAGIC_NEW2) {
		// new type, has extra header and irom section first
		romaddr = readpos + header->len + sizeof(rom_header_new);
#ifdef BOOT_IROM_CHKSUM
		// we will set the real section count later, when we read the header
		sectcount = 0xff;
		// just skip the first part of the header
		// rest is processed for the chksum
		readpos += sizeof(rom_header);
#else
		// skip the extra header and irom section
		readpos = romaddr;
		// read the normal header that follows
		if (SPIRead(readpos, header, sizeof(rom_header)) != 0) {
			return 0;
		}
		sectcount = header->count;
		readpos += sizeof(rom_header);
#endif
	} else {
		return 0;
	}

	// test each section
	for (sectcurrent = 0; sectcurrent < sectcount; sectcurrent++) {

		// read section header
		if (SPIRead(readpos, section, sizeof(section_header)) != 0) {
			return 0;
		}
		readpos += sizeof(section_header);

		// get section address and length
		remaining = section->length;

		while (remaining > 0) {
			// work out how much to read, up to BUFFER_SIZE
			uint32 readlen = (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE;
			// read the block
			if (SPIRead(readpos, buffer, readlen) != 0) {
				return 0;
			}
			// increment next read position
			readpos += readlen;
			// decrement remaining count
			remaining -= readlen;
			// add to chksum
			for (loop = 0; loop < readlen; loop++) {
				chksum ^= buffer[loop];
			}
		}

#ifdef BOOT_IROM_CHKSUM
		if (sectcount == 0xff) {
			// just processed the irom section, now
			// read the normal header that follows
			if (SPIRead(readpos, header, sizeof(rom_header)) != 0) {
				return 0;
			}
			sectcount = header->count + 1;
			readpos += sizeof(rom_header);
		}
#endif
	}

	// round up to next 16 and get checksum
	readpos = readpos | 0x0f;
	if (SPIRead(readpos, buffer, 1) != 0) {
		return 0;
	}

	// compare calculated and stored checksums
	if (buffer[0] != chksum) {
		return 0;
	}

	return romaddr;
}

#if defined (BOOT_GPIO_ENABLED) || defined(BOOT_GPIO_SKIP_ENABLED)

#if BOOT_GPIO_NUM > 16
#error "Invalid BOOT_GPIO_NUM value (disable BOOT_GPIO_ENABLED to disable this feature)"
#endif

// sample gpio code for gpio16
#define ETS_UNCACHED_ADDR(addr) (addr)
#define READ_PERI_REG(addr) (*((volatile uint32 *)ETS_UNCACHED_ADDR(addr)))
#define WRITE_PERI_REG(addr, val) (*((volatile uint32 *)ETS_UNCACHED_ADDR(addr))) = (uint32)(val)
#define PERIPHS_RTC_BASEADDR				0x60000700
#define REG_RTC_BASE  PERIPHS_RTC_BASEADDR
#define RTC_GPIO_OUT							(REG_RTC_BASE + 0x068)
#define RTC_GPIO_ENABLE							(REG_RTC_BASE + 0x074)
#define RTC_GPIO_IN_DATA						(REG_RTC_BASE + 0x08C)
#define RTC_GPIO_CONF							(REG_RTC_BASE + 0x090)
#define PAD_XPD_DCDC_CONF						(REG_RTC_BASE + 0x0A0)
static uint32 get_gpio16(void) {
	// set output level to 1
	WRITE_PERI_REG(RTC_GPIO_OUT, (READ_PERI_REG(RTC_GPIO_OUT) & (uint32)0xfffffffe) | (uint32)(1));

	// read level
	WRITE_PERI_REG(PAD_XPD_DCDC_CONF, (READ_PERI_REG(PAD_XPD_DCDC_CONF) & 0xffffffbc) | (uint32)0x1);	// mux configuration for XPD_DCDC and rtc_gpio0 connection
	WRITE_PERI_REG(RTC_GPIO_CONF, (READ_PERI_REG(RTC_GPIO_CONF) & (uint32)0xfffffffe) | (uint32)0x0);	//mux configuration for out enable
	WRITE_PERI_REG(RTC_GPIO_ENABLE, READ_PERI_REG(RTC_GPIO_ENABLE) & (uint32)0xfffffffe);	//out disable

	return (READ_PERI_REG(RTC_GPIO_IN_DATA) & 1);
}

// support for "normal" GPIOs (other than 16)
#define REG_GPIO_BASE            0x60000300
#define GPIO_IN_ADDRESS          (REG_GPIO_BASE + 0x18)
#define GPIO_ENABLE_OUT_ADDRESS  (REG_GPIO_BASE + 0x0c)
#define REG_IOMUX_BASE           0x60000800
#define IOMUX_PULLUP_MASK        (1<<7)
#define IOMUX_FUNC_MASK          0x0130
const uint8 IOMUX_REG_OFFS[] = {0x34, 0x18, 0x38, 0x14, 0x3c, 0x40, 0x1c, 0x20, 0x24, 0x28, 0x2c, 0x30, 0x04, 0x08, 0x0c, 0x10};
const uint8 IOMUX_GPIO_FUNC[] = {0x00, 0x30, 0x00, 0x30, 0x00, 0x00, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30};

static int get_gpio(int gpio_num) {
	// disable output buffer if set
	uint32 old_out = READ_PERI_REG(GPIO_ENABLE_OUT_ADDRESS);
	uint32 new_out = old_out & ~ (1<<gpio_num);
	WRITE_PERI_REG(GPIO_ENABLE_OUT_ADDRESS, new_out);

	// set GPIO function, enable soft pullup
	uint32 iomux_reg = REG_IOMUX_BASE + IOMUX_REG_OFFS[gpio_num];
	uint32 old_iomux = READ_PERI_REG(iomux_reg);
	uint32 gpio_func = IOMUX_GPIO_FUNC[gpio_num];
	uint32 new_iomux = (old_iomux & ~IOMUX_FUNC_MASK) | gpio_func | IOMUX_PULLUP_MASK;
	WRITE_PERI_REG(iomux_reg, new_iomux);

	// allow soft pullup to take effect if line was floating
	ets_delay_us(10);
	int result = READ_PERI_REG(GPIO_IN_ADDRESS) & (1<<gpio_num);

	// set iomux & GPIO output mode back to initial values
	WRITE_PERI_REG(iomux_reg, old_iomux);
	WRITE_PERI_REG(GPIO_ENABLE_OUT_ADDRESS, old_out);
	return (result ? 1 : 0);
}

// return '1' if we should do a gpio boot
static int perform_gpio_boot(rboot_config *romconf) {
	if (romconf->mode & MODE_GPIO_ROM == 0) {
		return FALSE;
	}

	// pin low == GPIO boot
	if (BOOT_GPIO_NUM == 16) {
		return (get_gpio16() == 0);
	} else {
		return (get_gpio(BOOT_GPIO_NUM) == 0);
	}
}

#endif

#ifdef BOOT_RTC_ENABLED
uint32 system_rtc_mem(int32 addr, void *buff, int32 length, uint32 mode) {

    int32 blocks;

    // validate reading a user block
    if (addr < 64) return 0;
    if (buff == 0) return 0;
    // validate 4 byte aligned
    if (((uint32)buff & 0x3) != 0) return 0;
    // validate length is multiple of 4
    if ((length & 0x3) != 0) return 0;

    // check valid length from specified starting point
    if (length > (0x300 - (addr * 4))) return 0;

    // copy the data
    for (blocks = (length >> 2) - 1; blocks >= 0; blocks--) {
        volatile uint32 *ram = ((uint32*)buff) + blocks;
        volatile uint32 *rtc = ((uint32*)0x60001100) + addr + blocks;
		if (mode == RBOOT_RTC_WRITE) {
			*rtc = *ram;
		} else {
			*ram = *rtc;
		}
    }

    return 1;
}
#endif

#ifdef BOOT_BAUDRATE
static enum rst_reason get_reset_reason(void) {

	// reset reason is stored @ offset 0 in system rtc memory
	volatile uint32 *rtc = (uint32*)0x60001100;

	return *rtc;
}
#endif

#if defined(BOOT_CONFIG_CHKSUM) || defined(BOOT_RTC_ENABLED)
// calculate checksum for block of data
// from start up to (but excluding) end
static uint8 calc_chksum(uint8 *start, uint8 *end) {
	uint8 chksum = CHKSUM_INIT;
	while(start < end) {
		chksum ^= *start;
		start++;
	}
	return chksum;
}
#endif

#ifndef BOOT_CUSTOM_DEFAULT_CONFIG
// populate the user fields of the default config
// created on first boot or in case of corruption
static uint8 default_config(rboot_config *romconf, uint32 flashsize) {
	romconf->count = 2;
	romconf->roms[0] = SECTOR_SIZE * (BOOT_CONFIG_SECTOR + 1);
	romconf->roms[1] = (flashsize / 2) + (SECTOR_SIZE * (BOOT_CONFIG_SECTOR + 1));
#ifdef BOOT_GPIO_ENABLED
	romconf->mode = MODE_GPIO_ROM;
#endif
#ifdef BOOT_GPIO_SKIP_ENABLED
	romconf->mode = MODE_GPIO_SKIP;
#endif
}
#endif

// prevent this function being placed inline with main
// to keep main's stack size as small as possible
// don't mark as static or it'll be optimised out when
// using the assembler stub
uint32 NOINLINE find_image(void) {

	uint8 flag;
	uint32 runAddr;
	uint32 flashsize;
	int32 romToBoot;
	uint8 updateConfig = FALSE;
	uint8 buffer[SECTOR_SIZE];
#ifdef BOOT_GPIO_ENABLED
	uint8 gpio_boot = FALSE;
#endif
#if defined (BOOT_GPIO_ENABLED) || defined(BOOT_GPIO_SKIP_ENABLED)
	uint8 sec;
#endif
#ifdef BOOT_RTC_ENABLED
	rboot_rtc_data rtc;
	uint8 temp_boot = FALSE;
#endif

	rboot_config *romconf = (rboot_config*)buffer;
	rom_header *header = (rom_header*)buffer;

#ifdef BOOT_BAUDRATE
	// soft reset doesn't reset PLL/divider, so leave as configured
	if (get_reset_reason() != REASON_SOFT_RESTART) {
		uart_div_modify( 0, UART_CLK_FREQ / BOOT_BAUDRATE);
	}
#endif

#if defined BOOT_DELAY_MICROS && BOOT_DELAY_MICROS > 0
	// delay to slow boot (help see messages when debugging)
	ets_delay_us(BOOT_DELAY_MICROS);
#endif

	ets_printf("\r\nrBoot v1.4.2 - richardaburton@gmail.com\r\n");

	// read rom header
	SPIRead(0, header, sizeof(rom_header));

	// print and get flash size
	ets_printf("Flash Size:   ");
	flag = header->flags2 >> 4;
	if (flag == 0) {
		ets_printf("4 Mbit\r\n");
		flashsize = 0x80000;
	} else if (flag == 1) {
		ets_printf("2 Mbit\r\n");
		flashsize = 0x40000;
	} else if (flag == 2) {
		ets_printf("8 Mbit\r\n");
		flashsize = 0x100000;
	} else if (flag == 3) {
		ets_printf("16 Mbit\r\n");
#ifdef BOOT_BIG_FLASH
		flashsize = 0x200000;
#else
		flashsize = 0x100000; // limit to 8Mbit
#endif
	} else if (flag == 4) {
		ets_printf("32 Mbit\r\n");
#ifdef BOOT_BIG_FLASH
		flashsize = 0x400000;
#else
		flashsize = 0x100000; // limit to 8Mbit
#endif
	} else {
		ets_printf("unknown\r\n");
		// assume at least 4mbit
		flashsize = 0x80000;
	}

	// print spi mode
	ets_printf("Flash Mode:   ");
	if (header->flags1 == 0) {
		ets_printf("QIO\r\n");
	} else if (header->flags1 == 1) {
		ets_printf("QOUT\r\n");
	} else if (header->flags1 == 2) {
		ets_printf("DIO\r\n");
	} else if (header->flags1 == 3) {
		ets_printf("DOUT\r\n");
	} else {
		ets_printf("unknown\r\n");
	}

	// print spi speed
	ets_printf("Flash Speed:  ");
	flag = header->flags2 & 0x0f;
	if (flag == 0) ets_printf("40 MHz\r\n");
	else if (flag == 1) ets_printf("26.7 MHz\r\n");
	else if (flag == 2) ets_printf("20 MHz\r\n");
	else if (flag == 0x0f) ets_printf("80 MHz\r\n");
	else ets_printf("unknown\r\n");

	// print enabled options
#ifdef BOOT_BIG_FLASH
	ets_printf("rBoot Option: Big flash\r\n");
#endif
#ifdef BOOT_CONFIG_CHKSUM
	ets_printf("rBoot Option: Config chksum\r\n");
#endif
#ifdef BOOT_GPIO_ENABLED
	ets_printf("rBoot Option: GPIO rom mode (%d)\r\n", BOOT_GPIO_NUM);
#endif
#ifdef BOOT_GPIO_SKIP_ENABLED
	ets_printf("rBoot Option: GPIO skip mode (%d)\r\n", BOOT_GPIO_NUM);
#endif
#ifdef BOOT_RTC_ENABLED
	ets_printf("rBoot Option: RTC data\r\n");
#endif
#ifdef BOOT_IROM_CHKSUM
	ets_printf("rBoot Option: irom chksum\r\n");
#endif
	ets_printf("\r\n");

	// read boot config
	SPIRead(BOOT_CONFIG_SECTOR * SECTOR_SIZE, buffer, SECTOR_SIZE);
	// fresh install or old version?
	if (romconf->magic != BOOT_CONFIG_MAGIC || romconf->version != BOOT_CONFIG_VERSION
#ifdef BOOT_CONFIG_CHKSUM
		|| romconf->chksum != calc_chksum((uint8*)romconf, (uint8*)&romconf->chksum)
#endif
		) {
		// create a default config for a standard 2 rom setup
		ets_printf("Writing default boot config.\r\n");
		ets_memset(romconf, 0x00, sizeof(rboot_config));
		romconf->magic = BOOT_CONFIG_MAGIC;
		romconf->version = BOOT_CONFIG_VERSION;
		default_config(romconf, flashsize);
#ifdef BOOT_CONFIG_CHKSUM
		romconf->chksum = calc_chksum((uint8*)romconf, (uint8*)&romconf->chksum);
#endif
		// write new config sector
		SPIEraseSector(BOOT_CONFIG_SECTOR);
		SPIWrite(BOOT_CONFIG_SECTOR * SECTOR_SIZE, buffer, SECTOR_SIZE);
	}

	// try rom selected in the config, unless overriden by gpio/temp boot
	romToBoot = romconf->current_rom;

#ifdef BOOT_RTC_ENABLED
	// if rtc data enabled, check for valid data
	if (system_rtc_mem(RBOOT_RTC_ADDR, &rtc, sizeof(rboot_rtc_data), RBOOT_RTC_READ) &&
		(rtc.chksum == calc_chksum((uint8*)&rtc, (uint8*)&rtc.chksum))) {

		if (rtc.next_mode & MODE_TEMP_ROM) {
			if (rtc.temp_rom >= romconf->count) {
				ets_printf("Invalid temp rom selected.\r\n");
				return 0;
			}
			ets_printf("Booting temp rom.\r\n");
			temp_boot = TRUE;
			romToBoot = rtc.temp_rom;
		}
	}
#endif

#if defined(BOOT_GPIO_ENABLED) || defined (BOOT_GPIO_SKIP_ENABLED)
	if (perform_gpio_boot(romconf)) {
#if defined(BOOT_GPIO_ENABLED)
		if (romconf->gpio_rom >= romconf->count) {
			ets_printf("Invalid GPIO rom selected.\r\n");
			return 0;
		}
		ets_printf("Booting GPIO-selected rom.\r\n");
		romToBoot = romconf->gpio_rom;
		gpio_boot = TRUE;
#elif defined(BOOT_GPIO_SKIP_ENABLED)
		romToBoot = romconf->current_rom + 1;
		if (romToBoot >= romconf->count) {
			romToBoot = 0;
		}
		romconf->current_rom = romToBoot;
#endif
		updateConfig = TRUE;
		if (romconf->mode & MODE_GPIO_ERASES_SDKCONFIG) {
			ets_printf("Erasing SDK config sectors before booting.\r\n");
			for (sec = 1; sec < 5; sec++) {
				SPIEraseSector((flashsize / SECTOR_SIZE) - sec);
			}
		}
	}
#endif

	// check valid rom number
	// gpio/temp boots will have already validated this
	if (romconf->current_rom >= romconf->count) {
		// if invalid rom selected try rom 0
		ets_printf("Invalid rom selected, defaulting to 0.\r\n");
		romToBoot = 0;
		romconf->current_rom = 0;
		updateConfig = TRUE;
	}

	// check rom is valid
	runAddr = check_image(romconf->roms[romToBoot]);

#ifdef BOOT_GPIO_ENABLED
	if (gpio_boot && runAddr == 0) {
		// don't switch to backup for gpio-selected rom
		ets_printf("GPIO boot rom (%d) is bad.\r\n", romToBoot);
		return 0;
	}
#endif
#ifdef BOOT_RTC_ENABLED
	if (temp_boot && runAddr == 0) {
		// don't switch to backup for temp rom
		ets_printf("Temp boot rom (%d) is bad.\r\n", romToBoot);
		// make sure rtc temp boot mode doesn't persist
		rtc.next_mode = MODE_STANDARD;
		rtc.chksum = calc_chksum((uint8*)&rtc, (uint8*)&rtc.chksum);
		system_rtc_mem(RBOOT_RTC_ADDR, &rtc, sizeof(rboot_rtc_data), RBOOT_RTC_WRITE);
		return 0;
	}
#endif

	// check we have a good rom
	while (runAddr == 0) {
		ets_printf("Rom %d is bad.\r\n", romToBoot);
		// for normal mode try each previous rom
		// until we find a good one or run out
		updateConfig = TRUE;
		romToBoot--;
		if (romToBoot < 0) romToBoot = romconf->count - 1;
		if (romToBoot == romconf->current_rom) {
			// tried them all and all are bad!
			ets_printf("No good rom available.\r\n");
			return 0;
		}
		runAddr = check_image(romconf->roms[romToBoot]);
	}

	// re-write config, if required
	if (updateConfig) {
		romconf->current_rom = romToBoot;
#ifdef BOOT_CONFIG_CHKSUM
		romconf->chksum = calc_chksum((uint8*)romconf, (uint8*)&romconf->chksum);
#endif
		SPIEraseSector(BOOT_CONFIG_SECTOR);
		SPIWrite(BOOT_CONFIG_SECTOR * SECTOR_SIZE, buffer, SECTOR_SIZE);
	}

#ifdef BOOT_RTC_ENABLED
	// set rtc boot data for app to read
	rtc.magic = RBOOT_RTC_MAGIC;
	rtc.next_mode = MODE_STANDARD;
	rtc.last_mode = MODE_STANDARD;
	if (temp_boot) rtc.last_mode |= MODE_TEMP_ROM;
#ifdef BOOT_GPIO_ENABLED
	if (gpio_boot) rtc.last_mode |= MODE_GPIO_ROM;
#endif
	rtc.last_rom = romToBoot;
	rtc.temp_rom = 0;
	rtc.chksum = calc_chksum((uint8*)&rtc, (uint8*)&rtc.chksum);
	system_rtc_mem(RBOOT_RTC_ADDR, &rtc, sizeof(rboot_rtc_data), RBOOT_RTC_WRITE);
#endif

	ets_printf("Booting rom %d.\r\n", romToBoot);
	// copy the loader to top of iram
	ets_memcpy((void*)_text_addr, _text_data, _text_len);
	// return address to load from
	return runAddr;

}

#ifdef BOOT_NO_ASM

// small stub method to ensure minimum stack space used
void call_user_start(void) {
	uint32 addr;
	stage2a *loader;

	addr = find_image();
	if (addr != 0) {
		loader = (stage2a*)entry_addr;
		loader(addr);
	}
}

#else

// assembler stub uses no stack space
// works with gcc
void call_user_start(void) {
	__asm volatile (
		"mov a15, a0\n"          // store return addr, hope nobody wanted a15!
		"call0 find_image\n"     // find a good rom to boot
		"mov a0, a15\n"          // restore return addr
		"bnez a2, 1f\n"          // ?success
		"ret\n"                  // no, return
		"1:\n"                   // yes...
		"movi a3, entry_addr\n"  // get pointer to entry_addr
		"l32i a3, a3, 0\n"       // get value of entry_addr
		"jx a3\n"                // now jump to it
	);
}

#endif
