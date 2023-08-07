/*
 * Advanced ESP Logger
 *
 * Copyright 2020-2022 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#include "adv_logger.h"

#include <stdio.h>
#include <string.h>

#ifdef ESP_PLATFORM

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "esp_attr.h"

#define sdk_system_get_time_raw()           esp_timer_get_time()

#else

#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_common.h>
#include <esplibs/libmain.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <stdout_redirect.h>
#include <semphr.h>

#endif

#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>
#include <lwip/dns.h>

#define HEADER_LEN                          (23)
#define UDP_LOG_LEN                         (1400)

// Task Stack Size
#ifdef ESP_PLATFORM
#define ADV_LOGGER_INIT_TASK_SIZE           (2048)
#define ADV_LOGGER_BUFFERED_TASK_SIZE       (2048)
#define ADV_LOGGER_VSNPRINTF_BUFFER_SIZE    (2048)
#else
#define ADV_LOGGER_INIT_TASK_SIZE           (configMINIMAL_STACK_SIZE)  // 256
#define ADV_LOGGER_BUFFERED_TASK_SIZE       (configMINIMAL_STACK_SIZE)  // 256
#endif

// Task Priority
#define ADV_LOGGER_INIT_TASK_PRIORITY       (tskIDLE_PRIORITY + 0)
#define ADV_LOGGER_BUFFERED_TASK_PRIORITY   (configMAX_PRIORITIES - 1)

#define ADV_LOGGER_ADDRESS                  "255.255.255.255"
#define ADV_LOGGER_PORT                     "45678"

typedef struct _adv_logger_data {
    int32_t socket;
    
    char* udplogstring;
    char* header;
    
#ifdef ESP_PLATFORM
    char* ip_address;
    SemaphoreHandle_t log_sender_semaphore;
#endif

    int8_t log_type: 4;
    uint16_t udplogstring_len: 11;
    uint16_t udplogstring_size: 11;
    bool is_new_line: 1;
    bool ready_to_send: 1;
    bool close_buffered_task: 1;
    
    TaskHandle_t xHandle;
    
    struct addrinfo* res;
} adv_logger_data_t;

static adv_logger_data_t* adv_logger_data = NULL;

static ssize_t adv_logger_write(struct _reent* r, int fd, const void* ptr, size_t len) {
#ifdef ESP_PLATFORM
    if (xSemaphoreTake(adv_logger_data->log_sender_semaphore, 0) == pdTRUE) {
#endif
        int is_adv_logger_data = false;
        if (adv_logger_data->ready_to_send) {
            is_adv_logger_data = true;
        }
        
        for (size_t i = 0; i < len; i++) {
            // Auto convert CR to CRLF, ignore other LFs (compatible with Espressif SDK behaviour)
            if (((char*) ptr)[i] == '\r') {
                continue;
            }
            
            if (adv_logger_data->udplogstring_len > (UDP_LOG_LEN - 35 - 1)) {
                is_adv_logger_data = false;
            }
            
            if (((char*) ptr)[i] == '\n') {
#ifdef ESP_PLATFORM
                char new_line_char = '\r';
                uart_write_bytes(0, &new_line_char, 1);
#else
                uart_putc(0, '\r');
#endif
                
                if (adv_logger_data->ready_to_send && is_adv_logger_data && adv_logger_data->udplogstring) {
                    adv_logger_data->udplogstring[adv_logger_data->udplogstring_len] = '\r';
                    adv_logger_data->udplogstring_len++;
                }
            }
            
#ifdef ESP_PLATFORM
            uart_write_bytes(0, &((char*) ptr)[i], 1);
#else
            uart_putc(0, ((char*) ptr)[i]);
#endif
            
            if (adv_logger_data->ready_to_send && is_adv_logger_data && adv_logger_data->udplogstring) {
                if (adv_logger_data->is_new_line) {
                    adv_logger_data->is_new_line = false;
                    adv_logger_data->udplogstring[adv_logger_data->udplogstring_len] = 0;
                    char buffer[10];
                    snprintf(buffer, 10, "%08.3f ", (float) sdk_system_get_time_raw() * 1e-6);
                    strcat(strcat(strcat(adv_logger_data->udplogstring, buffer), adv_logger_data->header), ": ");
                    adv_logger_data->udplogstring_len = strlen(adv_logger_data->udplogstring);
                }
                
                adv_logger_data->udplogstring[adv_logger_data->udplogstring_len] = ((char*) ptr)[i];
                adv_logger_data->udplogstring_len++;
                
                if (((char*) ptr)[i] == '\n') {
                    adv_logger_data->is_new_line = true;
                }
            }
        }
        
#ifdef ESP_PLATFORM
        xSemaphoreGive(adv_logger_data->log_sender_semaphore);
#endif
        
        return len;
        
#ifdef ESP_PLATFORM
    }
    
    return 0;
#endif
}

static void adv_logger_buffered_task() {
    unsigned int i = 0;
    
    for (;;) {
        i++;
        
        if ((i == 8 && adv_logger_data->udplogstring_len > 0) || adv_logger_data->udplogstring_len > (UDP_LOG_LEN / 2)) {
            adv_logger_data->ready_to_send = false;
            lwip_sendto(adv_logger_data->socket, adv_logger_data->udplogstring, adv_logger_data->udplogstring_len, 0, adv_logger_data->res->ai_addr, adv_logger_data->res->ai_addrlen);
            adv_logger_data->udplogstring_len = 0;
            adv_logger_data->ready_to_send = true;
            
            i = 0;
            
        } else if (i == 8) {
            i = 0;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
        
        if (adv_logger_data->close_buffered_task
#ifdef ESP_PLATFORM
        && xSemaphoreTake(adv_logger_data->log_sender_semaphore, pdMS_TO_TICKS(100)) == pdTRUE
#endif
        ) {
            
            free(adv_logger_data->udplogstring);
            adv_logger_data->udplogstring = NULL;
            
#ifdef ESP_PLATFORM
            xSemaphoreGive(adv_logger_data->log_sender_semaphore);
#endif
            
            vTaskDelete(NULL);
        }
    }
}

static void adv_logger_init_task(void* args) {
    uint8_t macaddr[6];
    
#ifdef ESP_PLATFORM
    while (!adv_logger_data->ip_address) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    esp_wifi_get_mac(WIFI_IF_STA, macaddr);
    snprintf(adv_logger_data->header, HEADER_LEN, "%s-%02X%02X%02X", adv_logger_data->ip_address, macaddr[3], macaddr[4], macaddr[5]);
    free(adv_logger_data->ip_address);
#else
    while (sdk_wifi_station_get_connect_status() != STATION_GOT_IP) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    struct ip_info info;
    sdk_wifi_get_ip_info(STATION_IF, &info);
    
    sdk_wifi_get_macaddr(STATION_IF, macaddr);
    snprintf(adv_logger_data->header, HEADER_LEN, IPSTR"-%02X%02X%02X", IP2STR(&info.ip), macaddr[3], macaddr[4], macaddr[5]);
#endif
    
    const struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_DGRAM,
    };
    
    while (getaddrinfo(ADV_LOGGER_ADDRESS, ADV_LOGGER_PORT, &hints, &adv_logger_data->res) != 0) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    while ((adv_logger_data->socket = lwip_socket(adv_logger_data->res->ai_family, adv_logger_data->res->ai_socktype, 0)) < 0) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    adv_logger_data->udplogstring = malloc(UDP_LOG_LEN);
    adv_logger_data->udplogstring[0] = 0;
    
    strcat(adv_logger_data->udplogstring, "\r\nAdvanced ESP Logger (c) 2022-2023 José A. Jiménez Campos\r\n\r\n");
    adv_logger_data->udplogstring_len = strlen(adv_logger_data->udplogstring);

    xTaskCreate(adv_logger_buffered_task, "LOG", ADV_LOGGER_BUFFERED_TASK_SIZE, NULL, ADV_LOGGER_BUFFERED_TASK_PRIORITY, &adv_logger_data->xHandle);
    
    adv_logger_data->is_new_line = true;
    adv_logger_data->ready_to_send = true;
    
    vTaskDelete(NULL);
}

#ifdef ESP_PLATFORM
void adv_logger_set_ip_address(char* ip_address) {
    adv_logger_data->ip_address = ip_address;
}

int IRAM_ATTR adv_logger_printf(const char* format, ...) {
    if (adv_logger_data) {
        char* buffer = malloc(ADV_LOGGER_VSNPRINTF_BUFFER_SIZE);
        if (buffer) {
            va_list va;
            va_start(va, format);
            vsnprintf(buffer, ADV_LOGGER_VSNPRINTF_BUFFER_SIZE - 1, format, va);
            va_end(va);
            
            int res = adv_logger_write(NULL, 0, (void*) buffer, strlen(buffer));
            
            free(buffer);
            
            return res;
        }
    }
    
    return 0;
}
#endif

void adv_logger_close_buffered_task() {
    adv_logger_data->close_buffered_task = true;
}

void adv_logger_init() {
    adv_logger_data = malloc(sizeof(adv_logger_data_t));
    memset(adv_logger_data, 0, sizeof(*adv_logger_data));
    
#ifdef ESP_PLATFORM
    adv_logger_data->log_sender_semaphore = xSemaphoreCreateMutex();
#endif
    
    adv_logger_data->socket = -1;
    adv_logger_data->header = malloc(HEADER_LEN);
    
    xTaskCreate(adv_logger_init_task, "LOG", ADV_LOGGER_INIT_TASK_SIZE, NULL, ADV_LOGGER_INIT_TASK_PRIORITY, NULL);
    
#ifndef ESP_PLATFORM
    set_write_stdout(adv_logger_write);
#endif
}
