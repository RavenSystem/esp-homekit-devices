/*
 * Advanced Logger
 *
 * Copyright 2020 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

/*
 * Based on UDPLogger from Life-Cycle-Manager (LCM) by HomeAccessoryKid (@HomeACcessoryKid),
 * licensed under Apache License 2.0.
 * https://github.com/HomeACcessoryKid/life-cycle-manager/blob/master/udplogger.c
 */

#include "adv_logger.h"

#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_common.h>
//#include <espressif/esp_system.h> //for timestamp report only
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <string.h>
#include <lwip/sockets.h>
#include <stdout_redirect.h>

#define HEADER_LEN                  (23)
#define UDP_LOG_LEN                 (2048 - HEADER_LEN)
#define UDP_LOG_SEND_MIN_LEN        (UDP_LOG_LEN / 4)
#define TRIES_BEFORE_FORCE_SEND     (20)

// Task Stack Size                  configMINIMAL_STACK_SIZE = 256
#define ADV_LOGGER_TASK_SIZE        (configMINIMAL_STACK_SIZE * 1.5)
#define ADV_LOGGER_TASK_PRIORITY    (tskIDLE_PRIORITY + 2)

#define DESTINATION_PORT            (28338)     // 45678 in reversed bytes
#define SOURCE_PORT                 (40109)     // 44444 in reversed bytes

typedef struct _adv_logger_data {
    char* udplogstring;
    char* header;
    uint16_t udplogstring_len;
    int8_t log_type;
    bool is_wifi_ready;
} adv_logger_data_t;

static adv_logger_data_t* adv_logger_data = NULL;

static ssize_t adv_logger_none(struct _reent *r, int fd, const void *ptr, size_t len) {
    return len;
}

static ssize_t adv_logger_write(struct _reent *r, int fd, const void *ptr, size_t len) {
    for (int i = 0; i < len; i++) {
        // Auto convert CR to CRLF, ignore other LFs (compatible with Espressif SDK behaviour)
        if (((char *)ptr)[i] == '\r') {
            continue;
        }
        
        if (((char *)ptr)[i] == '\n') {
            if (adv_logger_data->log_type >= 0) {
                uart_putc(adv_logger_data->log_type, '\r');
            }
            
            if (adv_logger_data->is_wifi_ready) {
                adv_logger_data->udplogstring[adv_logger_data->udplogstring_len] = '\r';
                adv_logger_data->udplogstring_len++;
            }
        }
        
        if (adv_logger_data->log_type >= 0) {
            uart_putc(adv_logger_data->log_type, ((char *)ptr)[i]);
        }
        
        if (adv_logger_data->is_wifi_ready && adv_logger_data->udplogstring_len < (UDP_LOG_LEN - HEADER_LEN)) {
            adv_logger_data->udplogstring[adv_logger_data->udplogstring_len] = ((char *)ptr)[i];
            adv_logger_data->udplogstring_len++;
            
            if (((char *)ptr)[i] == '\n') {
                adv_logger_data->udplogstring[adv_logger_data->udplogstring_len] = 0;
                strcat(strcat(adv_logger_data->udplogstring, adv_logger_data->header), ": ");
                adv_logger_data->udplogstring_len = strlen(adv_logger_data->udplogstring);
            }
        }
    }
    
    return len;
}

static void adv_logger_task() {
    int lSocket;
    uint8_t i = 0;
    struct sockaddr_in sLocalAddr, sDestAddr;
    struct ip_info info;

    while (sdk_wifi_station_get_connect_status() != STATION_GOT_IP) {
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
    
    if (sdk_wifi_get_ip_info(STATION_IF, &info)) {
        uint8_t macaddr[6];
        sdk_wifi_get_macaddr(STATION_IF, macaddr);
        snprintf(adv_logger_data->header, HEADER_LEN, IPSTR"-%02X%02X%02X", IP2STR(&info.ip), macaddr[3], macaddr[4], macaddr[5]);
        adv_logger_data->udplogstring_len = snprintf(adv_logger_data->udplogstring, HEADER_LEN + 2, "%s-: ", adv_logger_data->header);
    } else {
        free(adv_logger_data->header);
        adv_logger_data->header = malloc(1);
        adv_logger_data->header[0] = 0;
        adv_logger_data->udplogstring_len = 0;
    }
    
    adv_logger_data->is_wifi_ready = true;
    
    lSocket = lwip_socket(AF_INET, SOCK_DGRAM, 0);
    memset((char *)&sLocalAddr, 0, sizeof(sLocalAddr));
    memset((char *)&sDestAddr, 0, sizeof(sDestAddr));
    
    // Destination
    sDestAddr.sin_family = AF_INET;
    sDestAddr.sin_len = sizeof(sDestAddr);
    sDestAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    sDestAddr.sin_port = DESTINATION_PORT;
    
    // Source
    sLocalAddr.sin_family = AF_INET;
    sLocalAddr.sin_len = sizeof(sLocalAddr);
    sLocalAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    sLocalAddr.sin_port = SOURCE_PORT;
    lwip_bind(lSocket, (struct sockaddr*) &sLocalAddr, sizeof(sLocalAddr));

    for (;;) {
        if ((i == TRIES_BEFORE_FORCE_SEND && adv_logger_data->udplogstring_len > 0) || adv_logger_data->udplogstring_len > UDP_LOG_SEND_MIN_LEN) {
            lwip_sendto(lSocket, adv_logger_data->udplogstring, adv_logger_data->udplogstring_len, 0, (struct sockaddr *)&sDestAddr, sizeof(sDestAddr));
            adv_logger_data->udplogstring_len = 0;
        }
        
        if (i == TRIES_BEFORE_FORCE_SEND) {
            i = 0;
        }
        
        i++;
        
        vTaskDelay(20 / portTICK_PERIOD_MS); // With len > 1000 and delay = 20ms, we might handle 400kbps throughput
    }
}

void adv_logger_init(const uint8_t log_type) {
    if (log_type == ADV_LOGGER_NONE) {
        set_write_stdout(adv_logger_none);

    } else {
        adv_logger_data = malloc(sizeof(adv_logger_data_t));
        adv_logger_data->udplogstring_len = 0;
        adv_logger_data->log_type = log_type - 1;
        adv_logger_data->is_wifi_ready = false;

        if (adv_logger_data->log_type > 1) {
            adv_logger_data->log_type -= 3;
        }
        
        if (log_type > 2) {
            adv_logger_data->header = malloc(HEADER_LEN);
            adv_logger_data->udplogstring = malloc(UDP_LOG_LEN);
            
            xTaskCreate(adv_logger_task, "adv_logger_task", ADV_LOGGER_TASK_SIZE, NULL, ADV_LOGGER_TASK_PRIORITY, NULL);
        }
        
        set_write_stdout(adv_logger_write);
    }
}
