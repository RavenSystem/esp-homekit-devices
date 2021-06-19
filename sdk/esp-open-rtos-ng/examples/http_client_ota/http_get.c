#include "espressif/esp_common.h"
#include "esp/uart.h"

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "http_client_ota.h"
#include "ssid_config.h"

#define vTaskDelayMs(ms) vTaskDelay((ms) / portTICK_PERIOD_MS)

/*
 * How to test
 * cd test_file
 * python -m SimpleHTTPServer 8080
 * fill missing define SERVER and PORT, in your private_ssid_config.h
 * Ready for test.
 */


#define BINARY_PATH "/blink.bin"
#define SHA256_PATH "/blink.sha256"

// Default
#define SERVER "192.168.1.30"
#define PORT "8080"

#ifndef SERVER
    #error "Server address is not defined define it:`192.168.X.X`"
#endif

#ifndef PORT
    #error "Port is not defined example:`8080`"
#endif

static inline void ota_error_handling(OTA_err err) {
    printf("Error:");

    switch(err) {
    case OTA_DNS_LOOKUP_FALLIED:
        printf("DNS lookup has fallied\n");
        break;
    case OTA_SOCKET_ALLOCATION_FALLIED:
        printf("Impossible allocate required socket\n");
        break;
    case OTA_SOCKET_CONNECTION_FALLIED:
        printf("Server unreachable, impossible connect\n");
        break;
    case OTA_SHA_DONT_MATCH:
        printf("Sha256 sum does not fit downloaded sha256\n");
        break;
    case OTA_REQUEST_SEND_FALLIED:
        printf("Impossible send HTTP request\n");
        break;
    case OTA_DOWLOAD_SIZE_NOT_MATCH:
        printf("Dowload size don't match with server declared size\n");
        break;
    case OTA_ONE_SLOT_ONLY:
        printf("rboot has only one slot configured, impossible switch it\n");
        break;
    case OTA_FAIL_SET_NEW_SLOT:
        printf("rboot cannot switch between rom\n");
        break;
    case OTA_IMAGE_VERIFY_FALLIED:
        printf("Dowloaded image binary checsum is fallied\n");
        break;
    case OTA_UPDATE_DONE:
        printf("Ota has completed upgrade process, all ready for system software reset\n");
        break;
    case OTA_HTTP_OK:
        printf("HTTP server has response 200, Ok\n");
        break;
    case OTA_HTTP_NOTFOUND:
        printf("HTTP server has response 404, file not found\n");
        break;
    }
}

static void ota_task(void *PvParameter)
{
    // Wait until we have joined AP and are assigned an IP *
    while (sdk_wifi_station_get_connect_status() != STATION_GOT_IP)
        vTaskDelayMs(100);

    while (1) {
        OTA_err err;
        // Remake this task until ota work
        err = ota_update((ota_info *) PvParameter);

        ota_error_handling(err);

        if(err != OTA_UPDATE_DONE) {
            vTaskDelayMs(1000);
            printf("\n\n\n");
            continue;
        }

        vTaskDelayMs(1000);
        printf("Delay 1\n");
        vTaskDelayMs(1000);
        printf("Delay 2\n");
        vTaskDelayMs(1000);
        printf("Delay 3\n");

        printf("Reset\n");
        sdk_system_restart();
    }
}

static ota_info info = {
    .server      = SERVER,
    .port        = PORT,
    .binary_path = BINARY_PATH,
    .sha256_path = SHA256_PATH,
};

void user_init(void)
{
    uart_set_baud(0, 115200);
    printf("SDK version:%s\n", sdk_system_get_sdk_version());

    struct sdk_station_config config = {
        .ssid     = WIFI_SSID,
        .password = WIFI_PASS,
    };

    /* required to call wifi_set_opmode before station_set_config */
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&config);

    xTaskCreate(ota_task, "get_task", 4096, &info, 2, NULL);
}
