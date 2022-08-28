/* Implementation of libmain/app_main.o from the Espressif SDK.
 *
 * This contains most of the startup code for the SDK/OS, some event handlers,
 * etc.
 *
 * Part of esp-open-rtos
 * Copyright (C) 2015 Superhouse Automation Pty Ltd
 * BSD Licensed as described in the file LICENSE
 */

#include <string.h>
#include <FreeRTOS.h>
#include <task.h>
#include <lwip/tcpip.h>

#include "common_macros.h"
#include "xtensa_ops.h"
#include "esp/rom.h"
#include "esp/uart.h"
#include "esp/iomux_regs.h"
#include "esp/spi_regs.h"
#include "esp/dport_regs.h"
#include "esp/wdev_regs.h"
#include "esp/wdt_regs.h"
#include "esp/rtcmem_regs.h"
#include "esp/hwrand.h"
#include "os_version.h"

#include "espressif/esp_common.h"
#include "espressif/phy_info.h"
#include "esplibs/libmain.h"
#include "esplibs/libnet80211.h"
#include "esplibs/libphy.h"
#include "esplibs/libpp.h"
#include "sysparam.h"

/* This is not declared in any header file (but arguably should be) */

void user_init(void);

#define BOOT_INFO_SIZE 28

// These are the offsets of these values within the RTCMEM regions.  It appears
// that the ROM saves them to RTCMEM before calling us, and we pull them out of
// there to display them in startup messages (not sure why it works that way).
#define RTCMEM_BACKUP_PHY_VER  31
#define RTCMEM_SYSTEM_PP_VER   62

extern uint32_t _bss_start;
extern uint32_t _bss_end;

// user_init_flag -- .bss+0x0
uint8_t sdk_user_init_flag;

// info -- .bss+0x4
struct sdk_info_st sdk_info;

// xUserTaskHandle -- .bss+0x28
TaskHandle_t sdk_xUserTaskHandle;

// xWatchDogTaskHandle -- .bss+0x2c
TaskHandle_t sdk_xWatchDogTaskHandle;

/* Static function prototypes */

static void IRAM get_otp_mac_address(uint8_t *buf);
static void IRAM set_spi0_divisor(uint32_t divisor);
static void zero_bss(void);
static void init_networking(sdk_phy_info_t *phy_info, uint8_t *mac_addr);
static void init_g_ic(void);
static void user_start_phase2(void);
static void dump_flash_sector(uint32_t start_sector, uint32_t length);
static void dump_flash_config_sectors(uint32_t start_sector);

// .Lfunc001 -- .text+0x14
static void IRAM get_otp_mac_address(uint8_t *buf) {
    uint32_t otp_flags;
    uint32_t otp_id0, otp_id1;
    uint32_t otp_vendor_id;

    otp_flags = DPORT.OTP_CHIPID;
    otp_id1 = DPORT.OTP_MAC1;
    otp_id0 = DPORT.OTP_MAC0;
    if (!(otp_flags & 0x8000)) {
        //FIXME: do we really need this check?
        printf("Firmware ONLY supports ESP8266!!!\n");
        abort();
    }
    if (otp_id0 == 0 && otp_id1 == 0) {
        printf("empty otp\n");
        abort();
    }
    if (otp_flags & 0x1000) {
        // If bit 12 is set, it indicates that the vendor portion of the MAC
        // address is stored in DPORT.OTP_MAC2.
        otp_vendor_id = DPORT.OTP_MAC2;
        buf[0] = otp_vendor_id >> 16;
        buf[1] = otp_vendor_id >> 8;
        buf[2] = otp_vendor_id;
    } else {
        // If bit 12 is clear, there's no MAC vendor in DPORT.OTP_MAC2, so
        // default to the Espressif MAC vendor prefix instead.
        buf[1] = 0xfe;
        buf[0] = 0x18;
        buf[2] = 0x34;
    }
    buf[3] = otp_id1 >> 8;
    buf[4] = otp_id1;
    buf[5] = otp_id0 >> 24;
}

// .Lfunc002 -- .text+0xa0
static void IRAM set_spi0_divisor(uint32_t divisor) {
    int cycle_len, half_cycle_len, clkdiv;

    if (divisor < 2) {
        clkdiv = 0;
        SPI(0).CTRL0 |= SPI_CTRL0_CLOCK_EQU_SYS_CLOCK;
        IOMUX.CONF |= IOMUX_CONF_SPI0_CLOCK_EQU_SYS_CLOCK;
    } else {
        cycle_len = divisor - 1;
        half_cycle_len = (divisor / 2) - 1;
        clkdiv = VAL2FIELD(SPI_CTRL0_CLOCK_NUM, cycle_len)
               | VAL2FIELD(SPI_CTRL0_CLOCK_HIGH, half_cycle_len)
               | VAL2FIELD(SPI_CTRL0_CLOCK_LOW, cycle_len);
        SPI(0).CTRL0 &= ~SPI_CTRL0_CLOCK_EQU_SYS_CLOCK;
        IOMUX.CONF &= ~IOMUX_CONF_SPI0_CLOCK_EQU_SYS_CLOCK;
    }
    SPI(0).CTRL0 = SET_FIELD(SPI(0).CTRL0, SPI_CTRL0_CLOCK, clkdiv);
}


static void IRAM default_putc(char c) {
    uart_putc(0, c);
}

void init_newlib_locks(void);
extern uint8_t sdk_wDevCtrl[];
void nano_malloc_insert_chunk(void *start, size_t size);

// .text+0x258
void IRAM sdk_user_start(void) {
    uint32_t buf32[sizeof(struct sdk_g_ic_saved_st) / 4];
    uint8_t *buf8 = (uint8_t *)buf32;
    uint32_t flash_speed_divisor;
    uint32_t flash_sectors;
    uint32_t flash_size;
    int boot_slot;
    uint32_t cksum_magic;
    uint32_t cksum_len;
    uint32_t cksum_value;
    uint32_t ic_flash_addr;
    uint32_t sysparam_addr;
    sysparam_status_t status;

    SPI(0).USER0 |= SPI_USER0_CS_SETUP;
    sdk_SPIRead(0, buf32, 4);

    switch (buf8[3] & 0x0f) {
        case 0xf:  // 80 MHz
            flash_speed_divisor = 1;
            break;
        case 0x0:  // 40 MHz
            flash_speed_divisor = 2;
            break;
        case 0x1:  // 26 MHz
            flash_speed_divisor = 3;
            break;
        case 0x2:  // 20 MHz
            flash_speed_divisor = 4;
            break;
        default:  // Invalid -- Assume 40 MHz
            flash_speed_divisor = 2;
    }
    switch (buf8[3] >> 4) {
        case 0x0:   // 4 Mbit (512 KByte)
            flash_size = 524288;
            break;
        case 0x1:  // 2 Mbit (256 Kbyte)
            flash_size = 262144;
            break;
        case 0x2:  // 8 Mbit (1 Mbyte)
            flash_size = 1048576;
            break;
        case 0x3:  // 16 Mbit (2 Mbyte)
        case 0x5:  // 16 Mbit (2 Mbyte)
            flash_size = 2097152;
            break;
        case 0x4:  // 32 Mbit (4 Mbyte)
        case 0x6:  // 32 Mbit (4 Mbyte)
            flash_size = 4194304;
            break;
        case 0x8:  // 64 Mbit (8 Mbyte)
            flash_size = 8388608;
            break;
        case 0x9:  // 128 Mbit (16 Mbyte)
            flash_size = 16777216;
            break;
        default:   // Invalid -- Assume 4 Mbit (512 KByte)
            flash_size = 524288;
    }
    flash_sectors = flash_size / sdk_flashchip.sector_size;
    sdk_flashchip.chip_size = flash_size;
    set_spi0_divisor(flash_speed_divisor);
    sdk_SPIRead(flash_size - 4096, buf32, BOOT_INFO_SIZE);
    boot_slot = buf8[0] ? 1 : 0;
    cksum_magic = buf32[1];
    cksum_len = buf32[3 + boot_slot];
    cksum_value = buf32[5 + boot_slot];
    ic_flash_addr = (flash_sectors - 3 + boot_slot) * sdk_flashchip.sector_size;
    sdk_SPIRead(ic_flash_addr, buf32, sizeof(struct sdk_g_ic_saved_st));
    Cache_Read_Enable(0, 0, 1);
    zero_bss();
    sdk_os_install_putc1(default_putc);

    /* HACK Reclaim a region of unused bss from wdev.o. This would not be
     * necessary if the source code to wdev were available, and then it would
     * not be a fragmented area, but the extra memory is desparately needed and
     * it is in very useful dram. */
    nano_malloc_insert_chunk((void *)(sdk_wDevCtrl + 0x2190), 8000);

    init_newlib_locks();

    if (cksum_magic == 0xffffffff) {
        // No checksum required
    } else if ((cksum_magic == 0x55aa55aa) &&
            (sdk_system_get_checksum(buf8, cksum_len) == cksum_value)) {
        // Checksum OK
    } else {
        // Bad checksum or bad cksum_magic value
        dump_flash_config_sectors(flash_sectors - 4);
        //FIXME: should we halt here? (original SDK code doesn't)
    }
    memcpy(&sdk_g_ic.s, buf32, sizeof(struct sdk_g_ic_saved_st));

    // By default, put the sysparam region just below the config sectors at the
    // top of the flash space, and allowing one extra sector spare.
    sysparam_addr = flash_size - (5 + DEFAULT_SYSPARAM_SECTORS) * sdk_flashchip.sector_size;
    status = sysparam_init(sysparam_addr, flash_size);
    if (status == SYSPARAM_NOTFOUND) {
        status = sysparam_create_area(sysparam_addr, DEFAULT_SYSPARAM_SECTORS, false);
        if (status == SYSPARAM_OK) {
            status = sysparam_init(sysparam_addr, 0);
        }
    }
    if (status != SYSPARAM_OK) {
        printf("WARNING: Could not initialize sysparams (%d)!\n", status);
    }

    user_start_phase2();
}

// .text+0x3a8
void IRAM vApplicationStackOverflowHook(TaskHandle_t task, char *task_name) {
    printf("Task stack overflow (high water mark=%lu name=\"%s\")\n", uxTaskGetStackHighWaterMark(task), task_name);
}

// .text+0x3d8
void __attribute__((weak)) IRAM vApplicationIdleHook(void) {
    printf("idle %u\n", WDEV.SYS_TIME);
}

// .text+0x404
void __attribute__((weak)) IRAM vApplicationTickHook(void) {
    printf("tick %u\n", WDEV.SYS_TIME);
}

// .Lfunc005 -- .irom0.text+0x8
static void zero_bss(void) {
    uint32_t *addr;

    for (addr = &_bss_start; addr < &_bss_end; addr++) {
        *addr = 0;
    }
}

// .Lfunc006 -- .irom0.text+0x70
static void init_networking(sdk_phy_info_t *phy_info, uint8_t *mac_addr) {
    // The call to sdk_register_chipv6_phy appears to change the bus clock,
    // perhaps from 40MHz to 26MHz, at least it has such an effect on the uart
    // baud rate. The caller flushes the TX fifos.
    if (sdk_register_chipv6_phy(phy_info)) {
        printf("FATAL: sdk_register_chipv6_phy failed");
        abort();
    }

    // The boot rom initializes uart0 for a 115200 baud rate but the bus clock
    // does not appear to be as expected so the initial baud rate is actually
    // 74906. On a cold boot, to keep the 74906 baud rate the uart0 divisor
    // would need to changed here to 74906. On a warm boot the bus clock is
    // expected to have already been set so the boot baud rate is 115200.
    // Reset the rate here and settle on a 115200 baud rate.
    if (sdk_rst_if.reason > 0) {
        uart_set_baud(0, 115200);
        uart_set_baud(1, 115200);
    }

    sdk_phy_disable_agc();
    sdk_ieee80211_phy_init(sdk_g_ic.s.phy_mode);
    sdk_lmacInit();
    sdk_wDev_Initialize();
    sdk_pp_attach();
    sdk_ieee80211_ifattach(&sdk_g_ic, mac_addr);
    _xt_isr_mask(1);
    DPORT.DPORT0 = SET_FIELD(DPORT.DPORT0, DPORT_DPORT0_FIELD0, 1);
    sdk_pm_attach();
    sdk_phy_enable_agc();
    sdk_cnx_attach(&sdk_g_ic);
    sdk_wDevEnableRx();
}

// .Lfunc007 -- .irom0.text+0x148
static void init_g_ic(void) {
    if (sdk_g_ic.s.wifi_mode == 0xff) {
        sdk_g_ic.s.wifi_mode = 2;
    }
    sdk_wifi_softap_set_default_ssid();
    if (sdk_g_ic.s._unknown30d < 1 || sdk_g_ic.s._unknown30d > 14) {
        sdk_g_ic.s._unknown30d = 1;
    }
    if (sdk_g_ic.s._unknown544 < 100 || sdk_g_ic.s._unknown544 > 60000) {
        sdk_g_ic.s._unknown544 = 100;
    }
    if (sdk_g_ic.s._unknown30e == 1 || sdk_g_ic.s._unknown30e > 4) {
        sdk_g_ic.s._unknown30e = 0;
    }
    bzero(sdk_g_ic.s._unknown2ac, sizeof(sdk_g_ic.s._unknown2ac));
    if (sdk_g_ic.s._unknown30f > 1) {
        sdk_g_ic.s._unknown30f = 0;
    }
    if (sdk_g_ic.s._unknown310 > 4) {
        sdk_g_ic.s._unknown310 = 4;
    }
    if (sdk_g_ic.s.sta_ssid.ssid_length == 0xffffffff) {
        bzero(&sdk_g_ic.s.sta_ssid, sizeof(sdk_g_ic.s.sta_ssid));
        bzero(&sdk_g_ic.s.sta_password, sizeof(sdk_g_ic.s.sta_password));
    }
    sdk_g_ic.s.wifi_led_enable = 0;
    if (sdk_g_ic.s.sta_bssid_set > 1) {
        sdk_g_ic.s.sta_bssid_set = 0;
    }
    if (sdk_g_ic.s.ap_number > 5) {
        sdk_g_ic.s.ap_number = 1;
    }
    if (sdk_g_ic.s.phy_mode < 1 || sdk_g_ic.s.phy_mode > 3) {
       sdk_g_ic.s.phy_mode = PHY_MODE_11N;
    }
}

// .irom0.text+0x398
void sdk_wdt_init(void) {
    WDT.CTRL &= ~WDT_CTRL_ENABLE;
    DPORT.INT_ENABLE |= DPORT_INT_ENABLE_WDT;
    WDT.REG1 = 0x0000000b;
    WDT.REG2 = 0x0000000c;
    WDT.CTRL |= WDT_CTRL_FLAG3 | WDT_CTRL_FLAG4 | WDT_CTRL_FLAG5;
    WDT.CTRL = SET_FIELD(WDT.CTRL, WDT_CTRL_FIELD0, 0);
    WDT.CTRL |= WDT_CTRL_ENABLE;
    sdk_pp_soft_wdt_init();
}

extern void *xPortSupervisorStackPointer;

// .irom0.text+0x474
void sdk_user_init_task(void *params) {
    int phy_ver, pp_ver;

    /* The start up stack is not used after scheduling has started, so all of
     * the top area of RAM which was stack can be used for the dynamic heap. */
    xPortSupervisorStackPointer = (void *)0x40000000;

    sdk_ets_timer_init();
    printf("\nESP-Open-SDK ver: %s compiled @ %s %s\n", OS_VERSION_STR, __DATE__, __TIME__);
    phy_ver = RTCMEM_BACKUP[RTCMEM_BACKUP_PHY_VER] >> 16;
    printf("phy ver: %d, ", phy_ver);
    pp_ver = RTCMEM_SYSTEM[RTCMEM_SYSTEM_PP_VER];
    printf("pp ver: %d.%d\n\n", (pp_ver >> 8) & 0xff, pp_ver & 0xff);
    user_init();
    sdk_user_init_flag = 1;
    sdk_wifi_mode_set(sdk_g_ic.s.wifi_mode);
    if (sdk_g_ic.s.wifi_mode == STATION_MODE) {
        sdk_wifi_station_start();
        LOCK_TCPIP_CORE();
        netif_set_default(sdk_g_ic.v.station_netif_info->netif);
        UNLOCK_TCPIP_CORE();
    }
    if (sdk_g_ic.s.wifi_mode == SOFTAP_MODE) {
        sdk_wifi_softap_start();
        LOCK_TCPIP_CORE();
        netif_set_default(sdk_g_ic.v.softap_netif_info->netif);
        UNLOCK_TCPIP_CORE();
    }
    if (sdk_g_ic.s.wifi_mode == STATIONAP_MODE) {
        sdk_wifi_station_start();
        sdk_wifi_softap_start();
        LOCK_TCPIP_CORE();
        netif_set_default(sdk_g_ic.v.station_netif_info->netif);
        UNLOCK_TCPIP_CORE();
    }
    if (sdk_wifi_station_get_auto_connect()) {
        sdk_wifi_station_connect();
    }
    vTaskDelete(NULL);
}

extern void (*__init_array_start)(void);
extern void (*__init_array_end)(void);

// .Lfunc009 -- .irom0.text+0x5b4
static __attribute__((noinline)) void user_start_phase2(void) {
    uint8_t *buf;
    sdk_phy_info_t phy_info, default_phy_info;

    sdk_system_rtc_mem_read(0, &sdk_rst_if, sizeof(sdk_rst_if));
    if (sdk_rst_if.reason > 3) {
        // Bad reason. Probably garbage.
        bzero(&sdk_rst_if, sizeof(sdk_rst_if));
    }
    buf = malloc(sizeof(sdk_rst_if));
    bzero(buf, sizeof(sdk_rst_if));
    sdk_system_rtc_mem_write(0, buf, sizeof(sdk_rst_if));
    free(buf);
    sdk_sleep_reset_analog_rtcreg_8266();
    get_otp_mac_address(sdk_info.sta_mac_addr);
    sdk_wifi_softap_cacl_mac(sdk_info.softap_mac_addr, sdk_info.sta_mac_addr);
    sdk_info.softap_ipaddr.addr = 0x0104a8c0;  // 192.168.4.1
    sdk_info.softap_netmask.addr = 0x00ffffff; // 255.255.255.0
    sdk_info.softap_gw.addr = 0x0104a8c0;      // 192.168.4.1
    init_g_ic();

    read_saved_phy_info(&phy_info);
    get_default_phy_info(&default_phy_info);

    if (phy_info.version != default_phy_info.version) {
        /* Versions don't match, use default for PHY info
           (may be a blank config sector, or a new default version.)
         */
        memcpy(&phy_info, &default_phy_info, sizeof(sdk_phy_info_t));
    }

    // Wait for UARTs to finish sending anything in their queues.
    uart_flush_txfifo(0);
    uart_flush_txfifo(1);

    init_networking(&phy_info, sdk_info.sta_mac_addr);

    srand(hwrand()); /* seed libc rng */

    // Set intial CPU clock speed to 160MHz if necessary
    _Static_assert(configCPU_CLOCK_HZ == 80000000 || configCPU_CLOCK_HZ == 160000000, "FreeRTOSConfig must define initial clock speed as either 80MHz or 160MHz");
    sdk_system_update_cpu_freq(configCPU_CLOCK_HZ / 1000000);

    // Call gcc constructor functions
    void (**ctor)(void);
    for ( ctor = &__init_array_start; ctor != &__init_array_end; ++ctor) {
        (*ctor)();
    }

    tcpip_init(NULL, NULL);
    sdk_wdt_init();
    xTaskCreate(sdk_user_init_task, "uiT", 1024, 0, 14, &sdk_xUserTaskHandle);
    vTaskStartScheduler();
}

// .Lfunc010 -- .irom0.text+0x710
static void dump_flash_sector(uint32_t start_sector, uint32_t length) {
    uint8_t *buf;
    int bufsize, i;

    bufsize = (length + 3) & 0xfffc;
    buf = malloc(bufsize);
    sdk_spi_flash_read(start_sector * sdk_flashchip.sector_size, (uint32_t *)buf
, bufsize);
    for (i = 0; i < length; i++) {
        if ((i & 0xf) == 0) {
            if (i) {
                printf("\n");
            }
            printf("%04x:", i);
        }
        printf(" %02x", buf[i]);
    }
    printf("\n");
    free(buf);
}

// .Lfunc011 -- .irom0.text+0x790
static __attribute__((noinline)) void dump_flash_config_sectors(uint32_t start_sector) {
    printf("system param error\n");
    // Note: original SDK code didn't dump PHY info
    printf("phy_info:\n");
    dump_flash_sector(start_sector, sizeof(sdk_phy_info_t));
    printf("\ng_ic saved 0:\n");
    dump_flash_sector(start_sector + 1, sizeof(struct sdk_g_ic_saved_st));
    printf("\ng_ic saved 1:\n");
    dump_flash_sector(start_sector + 2, sizeof(struct sdk_g_ic_saved_st));
    printf("\nboot info:\n");
    dump_flash_sector(start_sector + 3, BOOT_INFO_SIZE);
}

