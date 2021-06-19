#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "arch/cc.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include <espressif/spi_flash.h>
#include <espressif/esp_system.h>
#include <espressif/esp_common.h>
#include <espressif/esp_system.h>
#include "mbedtls/sha256.h"
#include "http_client_ota.h"
#include "rboot-api.h"
#include "rboot.h"
#define MODULE "OTA"

#if defined(DEBUG)
# ifndef MODULE
#  error "Module not define"
# endif

    # define DEBUG_PRINT(fmt, args ...) \
        printf("[%s]\t" fmt "\n", MODULE, ## args)
#else
    # define DEBUG_PRINT(fmt, args ...) /* Don't do anything in release builds */
#endif

#define MAX_IMAGE_SIZE        0x100000 /*1MB images max at the moment */
#define READ_BUFFER_LEN       512

#define SHA256_SIZE_BIN       32
#define SHA256_SIZE_STR       SHA256_SIZE_BIN * 2
#define SHA256_CONV_STEP_SIZE 4

#if SECTOR_SIZE % READ_BUFFER_LEN != 0
# error "Incompatible SECTOR SIZE, with you current READ_BUFFER"
#endif

#define SECTOR_BUFFER_SIZE (SECTOR_SIZE)
#define vTaskDelayMs(ms) vTaskDelay((ms) / portTICK_PERIOD_MS)

static ota_info *ota_inf;
static mbedtls_sha256_context *sha256_ctx;

static uint32_t flash_offset;
static uint32_t flash_limits;

static unsigned char *SHA256_output;
static uint16_t *SHA256_dowload;
static char *SHA256_str;
static char *SHA256_wrt_ptr;

/**
 * CallBack called from Http Buffered client, for ota firmaware
 */
static unsigned int ota_firmaware_dowload_callback(char *buf, uint16_t size)
{
    if (ota_inf->sha256_path != NULL)
        mbedtls_sha256_update(sha256_ctx, (const unsigned char *) buf, size);

    if (flash_offset + size > flash_limits) {
        DEBUG_PRINT("Flash Limits override");
        return -1;
    }

    // Ready for flash device, the erase NANDFLASH Block
    if (flash_offset % SECTOR_SIZE == 0) {
        unsigned int sector;

        sector = flash_offset / SECTOR_SIZE;
        sdk_spi_flash_erase_sector(sector);
    }

    // Write into Flash
    sdk_spi_flash_write(flash_offset, (uint32_t *) buf, size);
    flash_offset += size;
    return 1;
}

static unsigned int SHA256_check_callback(char *buf, uint16_t size)
{
    int Current_SHA_Size;

    // Check that str does not contains other streing with SHA256
    if (size > SHA256_SIZE_STR)
        size = SHA256_SIZE_STR;

    Current_SHA_Size = SHA256_wrt_ptr - (char *) SHA256_str;

    if (!(Current_SHA_Size > SHA256_SIZE_STR)) {
        memcpy(SHA256_wrt_ptr, buf, size);
        SHA256_wrt_ptr += size;
    }
    return 1;
}

static void convert_SHA256_Str_TO_uint32(char *str, uint16_t *final_sha_bin)
{
    char tmp[SHA256_CONV_STEP_SIZE + 1];
    char *wrt_ptr;
    int i;

    wrt_ptr = str;
    for (i = 0; i < SHA256_SIZE_STR / SHA256_CONV_STEP_SIZE; i++) {
        uint16_t val;
        memset(tmp, 0, sizeof(tmp));
        memcpy(tmp, wrt_ptr, SHA256_CONV_STEP_SIZE);

        val = strtol(tmp, NULL, 16);

        final_sha_bin[i] = LWIP_PLATFORM_HTONS(val);

        wrt_ptr += SHA256_CONV_STEP_SIZE;
    }
}

OTA_err ota_update(ota_info *ota_info_par)
{
    Http_client_info http_inf;
    rboot_config rboot_config;
    HTTP_Client_State err;
    int slot;

    ota_inf = ota_info_par;

    // Malloc memory for work
    http_inf.buffer      = malloc(SECTOR_BUFFER_SIZE);
    http_inf.buffer_size = SECTOR_BUFFER_SIZE;
    http_inf.server      = ota_inf->server;
    http_inf.port        = ota_inf->port;

    // Check memory alignement, must be aligned
    if ((unsigned int) http_inf.buffer % sizeof(unsigned int)) {
        DEBUG_PRINT("Malloc return Unaligned memory");
        free(http_inf.buffer);
    }

    if (ota_inf->sha256_path != NULL) {
        sha256_ctx     = malloc(sizeof(mbedtls_sha256_context));
        SHA256_output  = malloc(SHA256_SIZE_BIN);
        SHA256_dowload = malloc(SHA256_SIZE_BIN);
        SHA256_str     = malloc(SHA256_SIZE_STR + 1);
        SHA256_wrt_ptr = SHA256_str;
        SHA256_str[SHA256_SIZE_STR] = '\0';
        mbedtls_sha256_init(sha256_ctx);
    }

    DEBUG_PRINT("HTTP client task starting");

    rboot_config = rboot_get_config();
    slot         = (rboot_config.current_rom + 1) % rboot_config.count;

    DEBUG_PRINT("Image will be saved in OTA slot %d", slot);
    if (slot == rboot_config.current_rom) {
        DEBUG_PRINT("Only one OTA slot is configured!");
        err = OTA_ONE_SLOT_ONLY;
        goto dealloc_all;
    }

    /* Validate the OTA slot parameter */
    if (rboot_config.current_rom == slot || rboot_config.count <= slot)
        DEBUG_PRINT("Current rom set to unknow value:%d", rboot_config.current_rom);

    // Calculate room limits
    flash_offset = rboot_config.roms[slot];
    flash_limits = flash_offset + MAX_IMAGE_SIZE;

    if (ota_inf->sha256_path != NULL) {
        // Setup for dowload sha256
        http_inf.path           = ota_inf->sha256_path;
        http_inf.final_cb       = SHA256_check_callback;
        http_inf.buffer_full_cb = SHA256_check_callback;

        memset(SHA256_dowload, 0, SHA256_SIZE_BIN);
        memset(SHA256_str, 0, SHA256_SIZE_STR);

        err = HttpClient_dowload(&http_inf);

        // Check if dowload success
        if (err != HTTP_OK)
            goto dealloc_all;

        convert_SHA256_Str_TO_uint32(SHA256_str, SHA256_dowload);
    }

    // Ping Wdog
    vTaskDelayMs(250);

    // Dowload Firmaware
    http_inf.path           = ota_inf->binary_path;
    http_inf.final_cb       = ota_firmaware_dowload_callback;
    http_inf.buffer_full_cb = ota_firmaware_dowload_callback;
    if (ota_inf->sha256_path != NULL)
        mbedtls_sha256_starts(sha256_ctx, 0);  // Start SHA256, not SHA224

    err = HttpClient_dowload(&http_inf);

    if (err != HTTP_OK)
        goto dealloc_all;

    if (ota_inf->sha256_path != NULL) {
        char com_res;
        mbedtls_sha256_finish(sha256_ctx, SHA256_output);
        mbedtls_sha256_free(sha256_ctx);

        com_res = !memcmp((void *) SHA256_output, (void *) SHA256_dowload, SHA256_SIZE_BIN);
        if (!com_res) {
            DEBUG_PRINT("SHA256 is not equal");
            err = HTTP_SHA_DONT_MATCH;
            goto dealloc_all;
        }
    }
    // Ping watch DOG
    vTaskDelayMs(500);
    {
        #define MESSAGE_MAX 120
        unsigned int Rboot_verified, boot_dimension;
        char error_message[MESSAGE_MAX];

        memset(error_message, 0, sizeof(error_message));
        // Start verify
        Rboot_verified = rboot_verify_image(rboot_config.roms[slot], &boot_dimension, (const char **) &error_message);
        if (Rboot_verified) {
            // Rom OK, call final callback for let inform user that all is ready for switch and reset.

            vPortEnterCritical();
            if (!rboot_set_current_rom(slot)) {
                vPortExitCritical();
                err = OTA_FAIL_SET_NEW_SLOT;
                goto dealloc_all;
            }
            vPortExitCritical();

            // Update success, software return HTTP_200
            err = OTA_UPDATE_DONE;
            goto dealloc_all;
        } else {
            DEBUG_PRINT("%s", error_message);
            err = OTA_IMAGE_VERIFY_FALLIED;
            goto dealloc_all;
        }
    }

dealloc_all:
    free(http_inf.buffer);

    if (ota_inf->sha256_path != NULL) {
        free(sha256_ctx);
        free(SHA256_str);
        free(SHA256_output);
        free(SHA256_dowload);
    }
    return err;
} /* ota_update */
