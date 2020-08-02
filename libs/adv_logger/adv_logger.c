/*
 * Advanced ESP Logger
 *
 * Copyright 2020 José Antonio Jiménez Campos (@RavenSystem)
 *
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

#define HEADER_LEN                      (23)
#define UDP_LOG_LEN                     (1472)

// Task Stack Size                      configMINIMAL_STACK_SIZE = 256
#define ADV_LOGGER_INIT_TASK_SIZE       (configMINIMAL_STACK_SIZE * 1)
#define ADV_LOGGER_TASK_SIZE            (configMINIMAL_STACK_SIZE * 1)

// Task Priority
#define ADV_LOGGER_INIT_TASK_PRIORITY   (tskIDLE_PRIORITY + 0)
#define ADV_LOGGER_TASK_PRIORITY        (tskIDLE_PRIORITY + 2)

#define DESTINATION_PORT                (28338)     // 45678 in reversed bytes
#define SOURCE_PORT                     (40109)     // 44444 in reversed bytes

typedef struct _adv_logger_data {
    char* udplogstring;
    char* header;
    TaskHandle_t xHandle;
    uint16_t udplogstring_len;
    int8_t log_type;
    uint8_t is_wifi_ready: 1;
    uint8_t is_new_line: 1;
} adv_logger_data_t;

static adv_logger_data_t* adv_logger_data = NULL;

static ssize_t adv_logger_none(struct _reent* r, int fd, const void* ptr, size_t len) {
    return len;
}

static ssize_t adv_logger_write(struct _reent* r, int fd, const void* ptr, size_t len) {
    for (int i = 0; i < len; i++) {
        // Auto convert CR to CRLF, ignore other LFs (compatible with Espressif SDK behaviour)
        if (((char*) ptr)[i] == '\r') {
            continue;
        }
        
        if (((char*) ptr)[i] == '\n') {
            if (adv_logger_data->log_type >= 0) {
                uart_putc(adv_logger_data->log_type, '\r');
            }
            
            if (adv_logger_data->is_wifi_ready) {
                adv_logger_data->udplogstring[adv_logger_data->udplogstring_len] = '\r';
                adv_logger_data->udplogstring_len++;
            }
        }
        
        if (adv_logger_data->log_type >= 0) {
            uart_putc(adv_logger_data->log_type, ((char*) ptr)[i]);
        }
        
        if (adv_logger_data->is_wifi_ready) {
            if (adv_logger_data->is_new_line) {
                adv_logger_data->is_new_line = false;
                adv_logger_data->udplogstring[adv_logger_data->udplogstring_len] = 0;
                char buffer[14];
                snprintf(buffer, 14, "%012.2Lf ", ((long double) sdk_system_get_time()) * 1e-6);
                strcat(strcat(strcat(adv_logger_data->udplogstring, buffer), adv_logger_data->header), ": ");
                adv_logger_data->udplogstring_len = strlen(adv_logger_data->udplogstring);
            }
            
            adv_logger_data->udplogstring[adv_logger_data->udplogstring_len] = ((char*) ptr)[i];
            adv_logger_data->udplogstring_len++;
            
            if (((char*) ptr)[i] == '\n' || adv_logger_data->udplogstring_len >= (UDP_LOG_LEN - 10)) {
                adv_logger_data->is_new_line = true;
                
                vTaskResume(adv_logger_data->xHandle);
            }
        }
    }
    
    return len;
}

static void adv_logger_task() {
    int lSocket;
    struct sockaddr_in sLocalAddr, sDestAddr;

    lSocket = lwip_socket(AF_INET, SOCK_DGRAM, 0);
    memset((char*) &sLocalAddr, 0, sizeof(sLocalAddr));
    memset((char*) &sDestAddr, 0, sizeof(sDestAddr));
    
    // Source
    sLocalAddr.sin_family = AF_INET;
    sLocalAddr.sin_len = sizeof(sLocalAddr);
    sLocalAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    sLocalAddr.sin_port = SOURCE_PORT;
    lwip_bind(lSocket, (struct sockaddr*) &sLocalAddr, sizeof(sLocalAddr));
    
    // Destination
    sDestAddr.sin_family = AF_INET;
    sDestAddr.sin_len = sizeof(sDestAddr);
    sDestAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    sDestAddr.sin_port = DESTINATION_PORT;

    for (;;) {
        vTaskSuspend(NULL);
        
        lwip_sendto(lSocket, adv_logger_data->udplogstring, adv_logger_data->udplogstring_len, 0, (struct sockaddr*) &sDestAddr, sizeof(sDestAddr));
        adv_logger_data->udplogstring_len = 0;
    }
}

static void adv_logger_init_task() {
    struct ip_info info;

    while (sdk_wifi_station_get_connect_status() != STATION_GOT_IP) {
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
    
    if (sdk_wifi_get_ip_info(STATION_IF, &info)) {
        uint8_t macaddr[6];
        sdk_wifi_get_macaddr(STATION_IF, macaddr);
        snprintf(adv_logger_data->header, HEADER_LEN, IPSTR"-%02X%02X%02X", IP2STR(&info.ip), macaddr[3], macaddr[4], macaddr[5]);
    } else {
        free(adv_logger_data->header);
        adv_logger_data->header = malloc(1);
        adv_logger_data->header[0] = 0;
        adv_logger_data->udplogstring_len = 0;
    }

    xTaskCreate(adv_logger_task, "adv_logger_task", ADV_LOGGER_TASK_SIZE, NULL, ADV_LOGGER_TASK_PRIORITY, &adv_logger_data->xHandle);
    
    strcat(adv_logger_data->udplogstring, "\r\nAdvanced ESP Logger (c) 2020 José Antonio Jiménez Campos\r\n\r\n");
    adv_logger_data->udplogstring_len = strlen(adv_logger_data->udplogstring);

    adv_logger_data->is_new_line = true;
    adv_logger_data->is_wifi_ready = true;
    
    vTaskDelete(NULL);
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
            adv_logger_data->udplogstring[0] = 0;
            adv_logger_data->is_new_line = false;
            
            xTaskCreate(adv_logger_init_task, "adv_logger_init_task", ADV_LOGGER_INIT_TASK_SIZE, NULL, ADV_LOGGER_INIT_TASK_PRIORITY, NULL);
        }
        
        set_write_stdout(adv_logger_write);
    }
}
