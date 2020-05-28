/*
 * Advanced Logger
 *
 * Copyright 2020 José Antonio Jiménez Campos (@RavenSystem)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// (c) 2018-2019 HomeAccessoryKid

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


typedef struct _adv_logger_data {
    char* udplogstring;
    char* ip_addr;
    uint16_t udplogstring_len;
    uint8_t log_type;
} adv_logger_data_t;

static adv_logger_data_t* adv_logger_data = NULL;

static ssize_t adv_logger_none(struct _reent *r, int fd, const void *ptr, size_t len) {
    return len;
}

static ssize_t adv_logger_write(struct _reent *r, int fd, const void *ptr, size_t len) {
    int8_t log_type = -1;
    bool is_udplog = false;
    
    if (adv_logger_data->log_type < 2) {
        log_type = adv_logger_data->log_type;
        
    } else if (adv_logger_data->log_type == 2) {
        is_udplog = true;
        
    } else if (adv_logger_data->log_type > 2) {
        log_type = adv_logger_data->log_type - 3;
        is_udplog = true;
    }
    
    for (int i = 0; i < len; i++) {
        // Auto convert CR to CRLF, ignore other LFs (compatible with Espressif SDK behaviour)
        if (((char *)ptr)[i] == '\r') {
            continue;
        }
        
        if (((char *)ptr)[i] == '\n') {
            if (log_type >= 0) {
                uart_putc(log_type, '\r');
            }
            
            if (is_udplog) {
                adv_logger_data->udplogstring[adv_logger_data->udplogstring_len] = '\r';
                adv_logger_data->udplogstring_len++;
            }
        }
        
        if (log_type >= 0) {
            uart_putc(log_type, ((char *)ptr)[i]);
        }
        
        if (is_udplog) {
            adv_logger_data->udplogstring[adv_logger_data->udplogstring_len] = ((char *)ptr)[i];
            adv_logger_data->udplogstring_len++;
            
            if (((char *)ptr)[i] == '\n') {
                adv_logger_data->udplogstring[adv_logger_data->udplogstring_len] = 0;
                strcat(strcat(adv_logger_data->udplogstring, adv_logger_data->ip_addr), ": ");
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

    lSocket = lwip_socket(AF_INET, SOCK_DGRAM, 0);
    memset((char *)&sLocalAddr, 0, sizeof(sLocalAddr));
    memset((char *)&sDestAddr, 0, sizeof(sDestAddr));
    
    // Destination
    sDestAddr.sin_family = AF_INET;
    sDestAddr.sin_len = sizeof(sDestAddr);
    sDestAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    sDestAddr.sin_port = 28338;     // = 45678; // Reversed bytes
    
    // Source
    sLocalAddr.sin_family = AF_INET;
    sLocalAddr.sin_len = sizeof(sLocalAddr);
    sLocalAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    sLocalAddr.sin_port = 40109;    // = 44444; // Reversed bytes
    lwip_bind(lSocket, (struct sockaddr*) &sLocalAddr, sizeof(sLocalAddr));

    for (;;) {
        if ((!i && adv_logger_data->udplogstring_len > (strlen(adv_logger_data->ip_addr) + 2)) || adv_logger_data->udplogstring_len > 700) {
            lwip_sendto(lSocket, adv_logger_data->udplogstring, adv_logger_data->udplogstring_len, 0, (struct sockaddr *)&sDestAddr, sizeof(sDestAddr));
            adv_logger_data->udplogstring_len = 0;
            i = 10;
        }
        
        if (!i) {
            i = 10; // Sends output every 100ms if not more than 700 bytes
        }
        
        i--;
        
        vTaskDelay(10 / portTICK_PERIOD_MS); // With len > 1000 and delay = 10ms, we might handle 800kbps throughput
    }
}

int adv_logger_init(const uint8_t log_type) {
    struct ip_info info;
    
    if (log_type == ADV_LOGGER_NONE) {
        set_write_stdout(adv_logger_none);

    } else {
        adv_logger_data = malloc(sizeof(adv_logger_data_t));
        adv_logger_data->udplogstring_len = 0;
        adv_logger_data->log_type = log_type - 1;
        
        if (log_type > 2) {
            if (sdk_wifi_get_ip_info(STATION_IF, &info)) {
                adv_logger_data->ip_addr = malloc(16);
                adv_logger_data->udplogstring = malloc(2900);
                
                snprintf(adv_logger_data->ip_addr, 16, IPSTR, IP2STR(&info.ip));

                adv_logger_data->udplogstring_len = snprintf(adv_logger_data->udplogstring, 18, "%s: ", adv_logger_data->ip_addr);
                
                xTaskCreate(adv_logger_task, "adv_logger_task", 256, NULL, 2, NULL);
            } else {
                free(adv_logger_data);
                
                return -1;
            }
        }
        
        set_write_stdout(adv_logger_write);
    }
    
    return 0;
}
