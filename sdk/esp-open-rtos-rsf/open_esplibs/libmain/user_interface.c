/* Recreated Espressif libmain user_interface.o contents.

   Copyright (C) 2015 Espressif Systems. Derived from MIT Licensed SDK libraries.
   BSD Licensed as described in the file LICENSE
*/

#include "FreeRTOS.h"
#include "task.h"
#include "string.h"

#include "lwip/dhcp.h"

#include "esp/types.h"
#include "esp/rom.h"
#include "esp/dport_regs.h"
#include "esp/rtcmem_regs.h"
#include "esp/iomux_regs.h"
#include "esp/sar_regs.h"
#include "esp/wdev_regs.h"
#include "esp/uart.h"
#include "esp/rtc_regs.h"
#include "esp/iomux.h"

#include "etstimer.h"
#include "espressif/sdk_private.h"
#include "espressif/esp_system.h"
#include "espressif/esp_wifi.h"
#include "espressif/esp_sta.h"
#include "espressif/esp_softap.h"
#include "espressif/esp_misc.h"
#include "espressif/osapi.h"
#include "espressif/user_interface.h"

#include "esplibs/libmain.h"
#include "esplibs/libpp.h"
#include "esplibs/libphy.h"
#include "esplibs/libnet80211.h"

// Structure for the data contained in the last sector of Flash which contains
// meta-info about the saved wifi param sectors.
struct param_dir_st {
    uint8_t current_sector;   // 0x00
    uint32_t cksum_magic;     // 0x04
    uint32_t save_count;      // 0x08
    uint32_t cksum_len[2];    // 0x0c
    uint32_t cksum_value[2];  // 0x14
};
_Static_assert(sizeof(struct param_dir_st) == 28, "param_dir_st is the wrong size");

enum sdk_dhcp_status sdk_dhcpc_flag = DHCP_STARTED;
bool sdk_cpu_overclock;
struct sdk_rst_info sdk_rst_if;
sdk_wifi_promiscuous_cb_t sdk_promiscuous_cb;

static uint8_t _system_upgrade_flag; // Ldata009

// Timer to execute a second phase of switching to a deep sleep
static ETSTimer deep_sleep_timer;

// Prototypes for static functions
static bool _check_boot_version(void);
static void _deep_sleep_phase2(void *timer_arg);
static struct netif *_get_netif(uint32_t mode);

// Linker-created values used by sdk_system_print_meminfo
extern uint8_t _data_start[], _data_end[];
extern uint8_t _rodata_start[], _rodata_end[];
extern uint8_t _bss_start[], _bss_end[];
extern uint8_t _heap_start[];

#define _rom_reset_vector ((void (*)(void))0x40000080)

void IRAM sdk_system_restart_in_nmi(void) {
    uint32_t buf[8];

    sdk_system_rtc_mem_read(0, buf, 32);
    if (buf[0] != 2) {
        memset(buf, 0, 32);
        buf[0] = 3;
        sdk_system_rtc_mem_write(0, buf, 32);
    }

    uart_flush_txfifo(0);
    uart_flush_txfifo(1);

    if (!sdk_NMIIrqIsOn) {
        portENTER_CRITICAL();
        do {
            DPORT.DPORT0 = SET_FIELD(DPORT.DPORT0, DPORT_DPORT0_FIELD0, 0);
        } while (DPORT.DPORT0 & 1);
    }

    ESPSAR.UNKNOWN_48 |= 3;
    DPORT.CLOCKGATE_WATCHDOG |= DPORT_CLOCKGATE_WATCHDOG_UNKNOWN_8;
    ESPSAR.UNKNOWN_48 &= ~3;
    DPORT.CLOCKGATE_WATCHDOG &= ~DPORT_CLOCKGATE_WATCHDOG_UNKNOWN_8;

    Wait_SPI_Idle(&sdk_flashchip);
    Cache_Read_Disable();
    DPORT.SPI_CACHE_RAM &= ~(DPORT_SPI_CACHE_RAM_BANK0 | DPORT_SPI_CACHE_RAM_BANK1);
    // This calls directly to 0x40000080, the "reset" exception vector address.
    _rom_reset_vector();
}

bool IRAM sdk_system_rtc_mem_write(uint32_t des_addr, void *src_addr, uint16_t save_size) {
    uint32_t volatile *src_buf = (uint32_t *)src_addr;

    if (des_addr > 191) {
        return false;
    }
    if ((intptr_t)src_addr & 3) {
        return false;
    }
    if ((768 - (des_addr * 4)) < save_size) {
        return false;
    }
    if ((save_size & 3) != 0) {
        save_size = (save_size & ~3) + 4;
    }
    for (uint8_t i = 0; i < (save_size >> 2); i++) {
        RTCMEM_SYSTEM[des_addr + i] = src_buf[i];
    }
    return true;
}

bool IRAM sdk_system_rtc_mem_read(uint32_t src_addr, void *des_addr, uint16_t save_size) {
    uint32_t *dest_buf = (uint32_t *)des_addr;

    if (src_addr > 191) {
        return false;
    }
    if ((intptr_t)des_addr & 3) {
        return false;
    }
    if ((768 - (src_addr * 4)) < save_size) {
        return false;
    }
    if ((save_size & 3) != 0) {
        save_size = (save_size & ~3) + 4;
    }
    for (uint8_t i = 0; i < (save_size >> 2); i++) {
        dest_buf[i] = RTCMEM_SYSTEM[src_addr + i];
    }
    return true;
}

void sdk_system_pp_recycle_rx_pkt(struct esf_buf *esf_buf) {
        sdk_ppRecycleRxPkt(esf_buf);
}

uint16_t sdk_system_adc_read(void) {
        return sdk_test_tout(false);
}

void sdk_system_restart(void) {
    if (sdk_wifi_get_opmode() != 2) {
        sdk_wifi_station_stop();
    }
    if (sdk_wifi_get_opmode() != 1) {
        sdk_wifi_softap_stop();
    }
    vTaskDelay(6);
    IOMUX_GPIO12 |= IOMUX_PIN_PULLUP;
    sdk_wDev_MacTim1SetFunc(sdk_system_restart_in_nmi);
    sdk_wDev_MacTim1Arm(3);
}

void sdk_system_restore(void) {
    struct sdk_g_ic_saved_st *buf;

    buf = malloc(sizeof(struct sdk_g_ic_saved_st));
    memset(buf, 0xff, sizeof(struct sdk_g_ic_saved_st));
    memcpy(buf, &sdk_g_ic.s, 8);
    sdk_wifi_param_save_protect(buf);
    free(buf);
}

uint8_t sdk_system_get_boot_version(void) {
    return sdk_g_ic.s.boot_info & 0x1f;
}

static bool _check_boot_version(void) {
    uint8_t ver = sdk_system_get_boot_version();
    if (ver < 3 || ver == 31) {
        printf("failed: need boot >= 1.3\n");
        return false;
    }
    return true;
}

int sdk_system_get_test_result(void) {
    if (_check_boot_version()) {
        return (sdk_g_ic.s.boot_info >> 5) & 1;
    } else {
        return -1;
    }
}

uint32_t sdk_system_get_userbin_addr(void) {
    uint8_t buf[8];
    uint16_t unknown_var = 0; //FIXME: read but never written?
    uint32_t addr;
    uint32_t flash_size_code;

    if (!(sdk_g_ic.s.boot_info >> 7)) {
        if (sdk_g_ic.s._unknown1d8 & 0x4) {
            addr = sdk_g_ic.s.user1_addr[0] | (sdk_g_ic.s.user1_addr[1] << 8) | 
(sdk_g_ic.s.user1_addr[2] << 16);
        } else {
            addr = sdk_g_ic.s.user0_addr[0] | (sdk_g_ic.s.user0_addr[1] << 8) | (sdk_g_ic.s.user0_addr[2] << 16);
        }
    } else {
        if (!sdk_system_upgrade_userbin_check()) {
            addr = 0x00001000;
        } else {
            sdk_spi_flash_read(0, (uint32_t *)buf, 8);
            flash_size_code = buf[3] >> 4;
            if (flash_size_code >= 2 && flash_size_code < 5) {
                flash_size_code = 0x81;
            } else if (flash_size_code == 1) {
                flash_size_code = 0x41;
            } else {
                // FIXME: In the original code, this loads from a local stack
                // variable, which is never actually assigned to anywhere.
                // It's unclear what this value is actually supposed to be.
                flash_size_code = unknown_var;
            }
            addr = flash_size_code << 12;
        }
    }
    return addr;
}

uint8_t sdk_system_get_boot_mode(void) {
    int boot_version = sdk_g_ic.s.boot_info & 0x1f;
    if (boot_version < 3 || boot_version == 0x1f) {
        return 1;
    }
    return sdk_g_ic.s.boot_info >> 7;
}

bool sdk_system_restart_enhance(uint8_t bin_type, uint32_t bin_addr) {
    uint32_t current_addr;

    if (!_check_boot_version()) {
        return false;
    }
    if (bin_type == 0) {
        current_addr = sdk_system_get_userbin_addr();
        printf("restart to use user bin @ %x\n", bin_addr);
        sdk_g_ic.s.user1_addr[0] = bin_addr;
        sdk_g_ic.s.user1_addr[1] = bin_addr >> 8;
        sdk_g_ic.s.user1_addr[2] = bin_addr >> 16;
        sdk_g_ic.s.user0_addr[0] = current_addr;
        sdk_g_ic.s.user0_addr[1] = current_addr >> 8;
        sdk_g_ic.s.user0_addr[2] = current_addr >> 16;
        sdk_g_ic.s._unknown1d8 = (sdk_g_ic.s._unknown1d8 & 0xfb) | 0x04;
        sdk_g_ic.s.boot_info &= 0x7f;
        sdk_wifi_param_save_protect(&sdk_g_ic.s);
        sdk_system_restart();
        return true;
    } else {
        if (bin_type != 1) {
            printf("don't supported type.\n");
            return false;
        }
        if (!sdk_system_get_test_result()) {
            printf("test already passed.\n");
            return false;
        }
       
        printf("reboot to use test bin @ %x\n", bin_addr);
        sdk_g_ic.s.user0_addr[0] = bin_addr;
        sdk_g_ic.s.user0_addr[1] = bin_addr >> 8;
        sdk_g_ic.s.user0_addr[2] = bin_addr >> 16;
        sdk_g_ic.s.boot_info &= 0xbf;
        sdk_wifi_param_save_protect(&sdk_g_ic.s);
        sdk_system_restart();
       
        return true;
    }
}

bool sdk_system_upgrade_userbin_set(uint8_t userbin) {
    uint8_t userbin_val, userbin_mask;
    uint8_t boot_ver = sdk_system_get_boot_version();

    if (userbin >= 2) {
        return false;
    } else {
        if (boot_ver == 2 || boot_ver == 0x1f) {
            userbin_val = userbin & 0x0f;
            userbin_mask = 0xf0;
        } else {
            userbin_val = userbin & 0x03;
            userbin_mask = 0xfc;
        }
        sdk_g_ic.s._unknown1d8 = (sdk_g_ic.s._unknown1d8 & userbin_mask) | userbin_val;
        return true;
    }
}

uint8_t sdk_system_upgrade_userbin_check(void) {
    uint8_t boot_ver = sdk_system_get_boot_version();
    if (boot_ver != 0x1f && boot_ver != 2) {
        if ((sdk_g_ic.s._unknown1d8 & 0x03) == 1) {
            if (sdk_g_ic.s._unknown1d8 & 0x4) {
                return 1;
            } else {
                return 0;
            }
        } else {
            if (sdk_g_ic.s._unknown1d8 & 0x4) {
                return 0;
            } else {
                return 1;
            }
        }
    } else {
        if ((sdk_g_ic.s._unknown1d8 & 0x0f) == 1) {
            return 1;
        } else {
            return 0;
        }
    }
}

bool sdk_system_upgrade_flag_set(uint8_t flag) {
    if (flag < 3) {
        _system_upgrade_flag = flag;
        return true;
    }
    return false;
}

uint8_t sdk_system_upgrade_flag_check(void) {
    return _system_upgrade_flag;
}

bool sdk_system_upgrade_reboot(void) {
    uint8_t boot_ver = sdk_system_get_boot_version();
    uint8_t new__unknown1d8;

    if (_system_upgrade_flag != 2) {
        return false;
    }
    printf("reboot to use");
    if (boot_ver != 2 && boot_ver != 0x1f) {
        sdk_g_ic.s.boot_info = (sdk_g_ic.s.boot_info & 0x7f) | 0x80;
        sdk_g_ic.s._unknown1d8 = (sdk_g_ic.s._unknown1d8 & 0xfb) | 0x04;
        if ((sdk_g_ic.s._unknown1d8 & 0x03) == 1) {
            printf("1\n");
            new__unknown1d8 = sdk_g_ic.s._unknown1d8 & 0xfc;
        } else {
            printf("2\n");
            new__unknown1d8 = (sdk_g_ic.s._unknown1d8 & 0xfc) | 0x01;
        }
    } else {
        if ((sdk_g_ic.s._unknown1d8 & 0x0f) == 1) {
            printf("1\n");
            new__unknown1d8 = sdk_g_ic.s._unknown1d8 & 0xf0;
        } else {
            printf("2\n");
            new__unknown1d8 = (sdk_g_ic.s._unknown1d8 & 0xf0) | 0x01;
        }
    }
    sdk_g_ic.s._unknown1d8 = new__unknown1d8;
    sdk_wifi_param_save_protect(&sdk_g_ic.s);
    sdk_system_restart();
    return true;
}

static void _deep_sleep_phase2(void *timer_arg) {
    uint32_t time_in_us = (uint32_t)timer_arg;

    printf("deep sleep %ds\n\n", time_in_us / 1000000);
    while (FIELD2VAL(UART_STATUS_TXFIFO_COUNT, UART(0).STATUS)) {}
    while (FIELD2VAL(UART_STATUS_TXFIFO_COUNT, UART(1).STATUS)) {}
    RTC.CTRL0 = 0;
    RTC.CTRL0 &= 0xffffbfff;
    RTC.CTRL0 |= 0x00000030;
    RTC._unknown44 = 0x00000004;
    RTC._unknownc = 0x00010010;
    RTC._unknown48 = (RTC._unknown48 & 0xffff01ff) | 0x0000fc00;
    RTC._unknown48 = (RTC._unknown48 & 0xfffffe00) | 0x00000080;
    RTC.COUNTER_ALARM = RTC.COUNTER + 136;
    RTC.RESET_REASON2 = 0x00000008;
    RTC.RESET_REASON0 = 0x00100000;
    sdk_os_delay_us(200);
    RTC.GPIO_CFG[2] = 0x00000011;
    RTC.GPIO_CFG[3] = 0x00000003;
    RTC._unknownc = 0x000640c8;
    RTC.CTRL0 &= 0xffffffcf;
    sdk_pm_rtc_clock_cali_proc();
    sdk_pm_set_sleep_time(time_in_us);
    RTC.GPIO_CFG[2] = 0x00000011;
    RTC.GPIO_CFG[3] = 0x00000003;
    DPORT.INT_ENABLE &= ~(DPORT_INT_ENABLE_WDT);
    _xt_isr_mask(1 << ETS_WDT_INUM);
    RTC._unknown40 = 0xffffffff;
    RTC._unknown44 = 0x00000020;
    RTC._unknown10 = 0x00000000;
    if (time_in_us == 0) {
        RTC.RESET_REASON2 = 0x00000000;
    } else {
        RTC.RESET_REASON2 = 0x00000008;
    }
    RTC.RESET_REASON0 = 0x00100000;
}

void sdk_system_deep_sleep(uint32_t time_in_us) {
    if (sdk_wifi_get_opmode() != 2) {
        sdk_wifi_station_stop();
    }
    if (sdk_wifi_get_opmode() != 1) {
        sdk_wifi_softap_stop();
    }
    sdk_os_timer_disarm(&sdk_sta_con_timer);

    // Originally deep sleep function reused sdk_sta_con_timer
    // but we can't mix functions sdk_ets_timer_ with sdk_os_timer_ for the
    // same timer. So now deep sleep function uses a separate timer.
    sdk_ets_timer_disarm(&deep_sleep_timer);
    sdk_ets_timer_setfn(&deep_sleep_timer, _deep_sleep_phase2, (void *)time_in_us);
    sdk_ets_timer_arm(&deep_sleep_timer, 100, 0);
}

bool sdk_system_update_cpu_freq(uint8_t freq) {
    if (freq == 80) {
        DPORT.CPU_CLOCK &= ~(DPORT_CPU_CLOCK_X2);
        sdk_os_update_cpu_frequency(80);
    } else if (freq == 160) {
        DPORT.CPU_CLOCK |= DPORT_CPU_CLOCK_X2;
        sdk_os_update_cpu_frequency(160);
    } else {
        return false;
    }
    return true;
}

uint8_t sdk_system_get_cpu_freq(void) {
    return sdk_os_get_cpu_frequency();
}

bool sdk_system_overclock(void) {
    if (sdk_system_get_cpu_freq() == 80) {
        sdk_cpu_overclock = true;
        sdk_system_update_cpu_freq(160);
        return true;
    }
    return false;
}

bool sdk_system_restoreclock(void) {
    if (sdk_system_get_cpu_freq() == 160 && sdk_cpu_overclock) {
        sdk_cpu_overclock = false;
        sdk_system_update_cpu_freq(80);
        return true;
    }
    return false;
}

uint32_t sdk_system_get_time(void) {
    return WDEV.SYS_TIME + sdk_WdevTimOffSet;
}

uint32_t sdk_system_relative_time(uint32_t reltime) {
    return WDEV.SYS_TIME - reltime;
}

void sdk_system_station_got_ip_set(struct ip4_addr *ip, struct ip4_addr *mask, struct ip4_addr *gw) {
    uint8_t *ip_bytes = (uint8_t *)&ip->addr;
    uint8_t *mask_bytes = (uint8_t *)&mask->addr;
    uint8_t *gw_bytes = (uint8_t *)&gw->addr;
    uint32_t gpio_mask;

    sdk_g_ic.v.station_netif_info->connect_status = STATION_GOT_IP;
    printf("ip:%d.%d.%d.%d,mask:%d.%d.%d.%d,gw:%d.%d.%d.%d", ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3], mask_bytes[0], mask_bytes[1], mask_bytes[2], mask_bytes[3], gw_bytes[0], gw_bytes[1], gw_bytes[2], gw_bytes[3]);
    printf("\n");
    if ((sdk_g_ic.s.wifi_led_enable == 1) && (sdk_g_ic.s.wifi_mode == 1)) {
        sdk_os_timer_disarm(&sdk_sta_con_timer);
        gpio_mask = 1 << sdk_g_ic.s.wifi_led_gpio;
        sdk_gpio_output_set(0, gpio_mask, gpio_mask, 0);
    }
}

extern void *xPortSupervisorStackPointer;

void sdk_system_print_meminfo(void) {
    uint8_t *heap_end = xPortSupervisorStackPointer;
    printf("%s: %p ~ %p, len: %d\n", "data  ", _data_start, _data_end, _data_end - _data_start);
    printf("%s: %p ~ %p, len: %d\n", "rodata", _rodata_start, _rodata_end, _rodata_end - _rodata_start);
    printf("%s: %p ~ %p, len: %d\n", "bss   ", _bss_start, _bss_end, _bss_end - _bss_start);
    printf("%s: %p ~ %p, len: %d\n", "heap  ", _heap_start, heap_end, heap_end - _heap_start);
}

uint32_t sdk_system_get_free_heap_size(void) {
    return xPortGetFreeHeapSize();
}

uint32_t sdk_system_get_chip_id(void) {
    uint32_t mac0 = DPORT.OTP_MAC0 & 0xff000000;
    uint32_t mac1 = DPORT.OTP_MAC1 & 0x00ffffff;
    return (mac1 << 8) | (mac0 >> 24);
}

uint32_t sdk_system_rtc_clock_cali_proc(void) {
    return sdk_pm_rtc_clock_cali_proc();
}
    
uint32_t sdk_system_get_rtc_time(void) {
    return RTC.COUNTER;
}

struct sdk_rst_info *sdk_system_get_rst_info(void) {
    return &sdk_rst_if;
}

struct netif *sdk_system_get_netif(uint32_t mode) {
    return _get_netif(mode);
}

static struct netif *_get_netif(uint32_t mode) {
    struct sdk_g_ic_netif_info *info;

    if (mode >= 2) {
        return NULL;
    }
    if (mode == 0) {
        info = sdk_g_ic.v.station_netif_info;
    } else {
        info = sdk_g_ic.v.softap_netif_info;
    }
    if (info) {
        return info->netif;
    }
    return NULL;
}

bool sdk_wifi_station_dhcpc_start(void) {
    struct netif *netif = _get_netif(0);
    if (sdk_wifi_get_opmode() == 2) {
        return false;
    }
    if (netif && sdk_dhcpc_flag == DHCP_STOPPED) {
        sdk_info.sta_ipaddr.addr = 0;
        sdk_info.sta_netmask.addr = 0;
        sdk_info.sta_gw.addr = 0;
        LOCK_TCPIP_CORE();
        netif_set_addr(netif, &sdk_info.sta_ipaddr, &sdk_info.sta_netmask, &sdk_info.sta_gw);
        if (dhcp_start(netif)) {
            UNLOCK_TCPIP_CORE();
            return false;
        }
        UNLOCK_TCPIP_CORE();
    }
    sdk_dhcpc_flag = DHCP_STARTED;
    return true;
}

bool sdk_wifi_station_dhcpc_stop(void) {
    struct netif *netif = _get_netif(0);
    if (sdk_wifi_get_opmode() == 2) {
        return false;
    }
    LOCK_TCPIP_CORE();
    if (netif && sdk_dhcpc_flag == DHCP_STARTED) {
        dhcp_stop(netif);
    }
    sdk_dhcpc_flag = DHCP_STOPPED;
    UNLOCK_TCPIP_CORE();
    return true;
}

enum sdk_dhcp_status sdk_wifi_station_dhcpc_status(void) {
    return sdk_dhcpc_flag;
}


#ifndef WIFI_PARAM_SAVE
#define WIFI_PARAM_SAVE 1
#endif

#if WIFI_PARAM_SAVE
static void wifi_save_protect(uint32_t sector, uint32_t sector_size, uint32_t *arg2, size_t size) {
    uint32_t *buffer = malloc(size);
    uint32_t offset = sector * sector_size;

    do  {
        sdk_spi_flash_erase_sector(sector);
        sdk_spi_flash_write(offset, arg2, size);
        sdk_spi_flash_read(offset, buffer, size);
        if (memcmp(buffer, arg2, size) == 0) {
            break;
        }
        printf("[W]sec %d error\n", sector);
    } while (1);

    free(buffer);
}
#endif

void sdk_wifi_param_save_protect(struct sdk_g_ic_saved_st *params) {
#if WIFI_PARAM_SAVE
    uint32_t sector_size = sdk_flashchip.sector_size;
    uint32_t sectors = sdk_flashchip.chip_size / sector_size;

    uint32_t dir_sector = sectors - 1;
    struct param_dir_st dir;
    sdk_spi_flash_read(dir_sector * sector_size, (uint32_t *)&dir, sizeof(dir));
    uint8_t current_sector = dir.current_sector ? 1 : 0;
    dir.current_sector = current_sector;

    uint32_t param_sector_start = sectors - 3;
    wifi_save_protect(param_sector_start + current_sector, sector_size,
                      (uint32_t *)params, sizeof(struct sdk_g_ic_saved_st));

    dir.cksum_magic = 0x55AA55AA;
    uint32_t save_count = dir.save_count + 1;
    dir.save_count = (save_count) ? save_count : 1;
    dir.cksum_len[current_sector] = sizeof(dir);
    uint32_t checksum = sdk_system_get_checksum((uint8_t *)params, sizeof(dir));
    dir.cksum_value[current_sector] = checksum;
    wifi_save_protect(dir_sector, sector_size, (uint32_t *)&dir, sizeof(dir));
#endif
}


uint8_t sdk_wifi_station_get_connect_status() {
    if (sdk_wifi_get_opmode() == 2) // ESPCONN_AP
        return 0xff;

    struct sdk_g_ic_netif_info *netif_info = sdk_g_ic.v.station_netif_info;
    if (!netif_info)
        return 0xff;

    return netif_info->connect_status;
}

bool sdk_wifi_get_ip_info(uint8_t if_index, struct ip_info *info) {
    if (if_index >= 2) return false;
    if (!info) return false;
    struct netif *netif = _get_netif(if_index);
    if (netif) {
        ip4_addr_set(&info->ip, ip_2_ip4(&netif->ip_addr));
        ip4_addr_set(&info->netmask, ip_2_ip4(&netif->netmask));
        ip4_addr_set(&info->gw, ip_2_ip4(&netif->gw));
        return true;
    }

    info->ip.addr = 0;
    info->netmask.addr = 0;
    info->gw.addr = 0;
    return false;
}

bool sdk_wifi_set_ip_info(uint8_t if_index, struct ip_info *info) {
    if (if_index >= 2) return false;
    if (!info) return false;

    if (if_index != 0) {
        sdk_info.softap_ipaddr = info->ip;
        sdk_info.softap_netmask = info->netmask;
        sdk_info.softap_gw = info->gw;
    } else {
        if (sdk_dhcpc_flag == 1 && sdk_user_init_flag == 1)
            return false;
        sdk_info.sta_ipaddr = info->ip;
        sdk_info.sta_netmask = info->netmask;
        sdk_info.sta_gw = info->gw;
    }

    struct netif *netif = _get_netif(if_index);
    if (netif) {
        LOCK_TCPIP_CORE();
        netif_set_addr(netif, &info->ip, &info->netmask, &info->gw);
        UNLOCK_TCPIP_CORE();
    }

    return true;
}

bool sdk_wifi_get_macaddr(uint8_t if_index, uint8_t *macaddr) {
    if (if_index >= 2) return false;
    if (!macaddr) return false;

    struct netif *netif = _get_netif(if_index);
    if (!netif) {
        if (if_index != 0) {
            memcpy(macaddr, sdk_info.softap_mac_addr, 6);
            return true;
        }
        memcpy(macaddr, sdk_info.sta_mac_addr, 6);
        return true;
    }
    memcpy(macaddr, netif->hwaddr, 6);
    return true;
}

bool sdk_wifi_set_macaddr(uint8_t if_index, uint8_t *macaddr) {
    if (if_index >= 2) return false;
    if (!macaddr) return false;

    struct netif *netif = _get_netif(if_index);
    uint8_t mode = sdk_wifi_get_opmode();

    if (if_index == 0) {
        if (mode == STATION_MODE) return false;
        if (memcmp(sdk_info.softap_mac_addr, macaddr, 6)) {
            memcpy(sdk_info.softap_mac_addr, macaddr, 6);
            if (netif) {
                memcpy(netif->hwaddr, macaddr, 6);
                sdk_wifi_softap_stop();
                sdk_wifi_softap_start();
            }
        }
        return true;
    }

    if (mode == SOFTAP_MODE) return false;
    if (memcmp(sdk_info.sta_mac_addr, macaddr, 6)) {
        memcpy(sdk_info.sta_mac_addr, macaddr, 6);
        if (netif) {
            memcpy(netif->hwaddr, macaddr, 6);
            sdk_wifi_station_stop();
            sdk_wifi_station_start();
            sdk_wifi_station_connect();
        }
    }

    return true;
}

void sdk_system_uart_swap()
{
    uart_flush_txfifo(0);
    uart_flush_txfifo(1);

    /* Disable pullup IO_MUX_MTDO, Alt TX. GPIO15. */
    iomux_set_pullup_flags(3, 0);
    /* IO_MUX_MTDO to function UART0_RTS. */
    iomux_set_function(3, IOMUX_GPIO15_FUNC_UART0_RTS);
    /* Enable pullup MUX_MTCK. Alt RX. GPIO13. */
    iomux_set_pullup_flags(1, IOMUX_PIN_PULLUP);
    /* IO_MUX_MTCK to function UART0_CTS. */
    iomux_set_function(1, IOMUX_GPIO13_FUNC_UART0_CTS);

    DPORT.PERI_IO |= DPORT_PERI_IO_SWAP_UART0_PINS;
}

void sdk_system_uart_de_swap()
{
    uart_flush_txfifo(0);
    uart_flush_txfifo(1);

    /* Disable pullup IO_MUX_U0TXD, TX. GPIO 1. */
    iomux_set_pullup_flags(5, 0);
    /* IO_MUX_U0TXD to function UART0_TXD. */
    iomux_set_function(5, IOMUX_GPIO1_FUNC_UART0_TXD);
    /* Enable pullup IO_MUX_U0RXD. RX. GPIO 3. */
    iomux_set_pullup_flags(4, IOMUX_PIN_PULLUP);
    /* IO_MUX_U0RXD to function UART0_RXD. */
    iomux_set_function(4, IOMUX_GPIO3_FUNC_UART0_RXD);

    DPORT.PERI_IO &= ~DPORT_PERI_IO_SWAP_UART0_PINS;
}

enum sdk_sleep_type sdk_wifi_get_sleep_type()
{
    return sdk_pm_get_sleep_type();
}

bool sdk_wifi_set_sleep_type(enum sdk_sleep_type type)
{
    if (type > WIFI_SLEEP_MODEM) return false;
    sdk_pm_set_sleep_type_from_upper(type);
    return true;
}
