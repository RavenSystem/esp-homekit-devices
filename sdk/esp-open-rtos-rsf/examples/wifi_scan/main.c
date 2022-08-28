/*
 * WiFi scan
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Ruslan V. Uss <unclerus@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */
#include <esp/uart.h>
#include <stdio.h>
#include <espressif/esp_common.h>
#include <FreeRTOS.h>
#include <task.h>
#include <string.h>

static const char * const auth_modes [] = {
    [AUTH_OPEN]         = "Open",
    [AUTH_WEP]          = "WEP",
    [AUTH_WPA_PSK]      = "WPA/PSK",
    [AUTH_WPA2_PSK]     = "WPA2/PSK",
    [AUTH_WPA_WPA2_PSK] = "WPA/WPA2/PSK"
};

static void scan_done_cb(void *arg, sdk_scan_status_t status)
{
    char ssid[33]; // max SSID length + zero byte

    if (status != SCAN_OK)
    {
        printf("Error: WiFi scan failed\n");
        return;
    }

    struct sdk_bss_info *bss = (struct sdk_bss_info *)arg;
    // first one is invalid
    bss = bss->next.stqe_next;

    printf("\n----------------------------------------------------------------------------------\n");
    printf("                             Wi-Fi networks\n");
    printf("----------------------------------------------------------------------------------\n");

    while (NULL != bss)
    {
        size_t len = strlen((const char *)bss->ssid);
        memcpy(ssid, bss->ssid, len);
        ssid[len] = 0;

        printf("%32s (" MACSTR ") RSSI: %02d, security: %s\n", ssid,
            MAC2STR(bss->bssid), bss->rssi, auth_modes[bss->authmode]);

        bss = bss->next.stqe_next;
    }
}

static void scan_task(void *arg)
{
    while (true)
    {
        sdk_wifi_station_scan(NULL, scan_done_cb);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void user_init()
{
    uart_set_baud(0, 115200);
    printf("SDK version:%s\n\n", sdk_system_get_sdk_version());

    // We can scan only in station mode
    sdk_wifi_set_opmode(STATION_MODE);

    xTaskCreate(scan_task, "scan", 512, NULL, 2, NULL);
}
