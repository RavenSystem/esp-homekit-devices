/*
 * Advanced ESP Logger with NTP support
 *
 * Copyright 2020-2021 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#include "adv_logger_ntp.h"

#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_common.h>
#include <esplibs/libmain.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <string.h>
#include <stdout_redirect.h>
#include <semphr.h>

#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>
#include <lwip/dns.h>

#include <raven_ntp.h>

#define HEADER_LEN                          (23)
#define UDP_LOG_LEN                         (1472)

// Task Stack Size                          configMINIMAL_STACK_SIZE = 256
#define ADV_LOGGER_INIT_TASK_SIZE           (configMINIMAL_STACK_SIZE)
#define ADV_LOGGER_BUFFERED_TASK_SIZE       (configMINIMAL_STACK_SIZE)

// Task Priority
#define ADV_LOGGER_INIT_TASK_PRIORITY       (tskIDLE_PRIORITY + 0)
#define ADV_LOGGER_BUFFERED_TASK_PRIORITY   (configMAX_PRIORITIES)

#define ADV_LOGGER_DEFAULT_DESTINATION      "255.255.255.255:45678"

typedef struct _adv_logger_data {
    int socket;
    
    char* udplogstring;
    char* header;

    int8_t log_type: 4;
    uint16_t udplogstring_len: 11;
    uint16_t udplogstring_size: 11;
    bool is_new_line: 1;
    bool ready_to_send: 1;
    bool is_buffered: 1;
    
    TaskHandle_t xHandle;
    SemaphoreHandle_t log_sender_semaphore;
    
    struct addrinfo* res;
} adv_logger_data_t;

static adv_logger_data_t* adv_logger_data = NULL;
static _WriteFunction* adv_logger_original_write_function = NULL;

static ssize_t adv_logger_none(struct _reent* r, int fd, const void* ptr, size_t len) {
    return len;
}

static ssize_t adv_logger_write(struct _reent* r, int fd, const void* ptr, size_t len) {
    bool is_adv_logger_data = false;
    if (adv_logger_data->ready_to_send) {
        if (adv_logger_data->is_buffered) {
            is_adv_logger_data = true;
        } else {
            if (!adv_logger_data->udplogstring) {
                adv_logger_data->udplogstring_size = len + 1;
                adv_logger_data->udplogstring = malloc(len);
                if (adv_logger_data->udplogstring) {
                    adv_logger_data->udplogstring[0] = 0;
                    is_adv_logger_data = true;
                }
            } else {
                adv_logger_data->udplogstring_size += len;
                if (adv_logger_data->udplogstring_size <= UDP_LOG_LEN) {
                    char* new_udplogstring = realloc(adv_logger_data->udplogstring, adv_logger_data->udplogstring_size);
                    if (new_udplogstring) {
                        adv_logger_data->udplogstring = new_udplogstring;
                        is_adv_logger_data = true;
                    }
                }
            }
        }
    }
    
    char buffer[17];
    
    for (int i = 0; i < len; i++) {
        // Auto convert CR to CRLF, ignore other LFs (compatible with Espressif SDK behaviour)
        if (((char*) ptr)[i] == '\r') {
            continue;
        }
        
        if (((char*) ptr)[i] == '\n') {
            if (adv_logger_data->log_type >= 0) {
                uart_putc(adv_logger_data->log_type, '\r');
            }
            
            if (adv_logger_data->ready_to_send) {
                adv_logger_data->udplogstring[adv_logger_data->udplogstring_len] = '\r';
                adv_logger_data->udplogstring_len++;
            }
        }
        
        if (adv_logger_data->is_new_line) {
            raven_ntp_get_log_time(buffer, 16);
            buffer[15] = ' ';
            buffer[16] = 0;
        }
        
        if (adv_logger_data->log_type >= 0) {
            if (adv_logger_data->is_new_line) {
                for (uint8_t j = 0; j < 16; j++) {
                    uart_putc(adv_logger_data->log_type, buffer[j]);
                }
            }
            
            uart_putc(adv_logger_data->log_type, ((char*) ptr)[i]);
        }
        
        if (adv_logger_data->ready_to_send && is_adv_logger_data) {
            if (adv_logger_data->is_new_line) {
                if (!adv_logger_data->is_buffered) {
                    adv_logger_data->udplogstring_size += 40;
                    if (adv_logger_data->udplogstring_size > UDP_LOG_LEN) {
                        is_adv_logger_data = false;
                        continue;
                    }
                    
                    char* new_udplogstring = realloc(adv_logger_data->udplogstring, adv_logger_data->udplogstring_size);
                    if (new_udplogstring) {
                        adv_logger_data->udplogstring = new_udplogstring;
                    } else {
                        is_adv_logger_data = false;
                        continue;
                    }
                }
                
                adv_logger_data->udplogstring[adv_logger_data->udplogstring_len] = 0;
                strcat(strcat(strcat(adv_logger_data->udplogstring, buffer), adv_logger_data->header), " ");
                adv_logger_data->udplogstring_len = strlen(adv_logger_data->udplogstring);
            }

            adv_logger_data->udplogstring[adv_logger_data->udplogstring_len] = ((char*) ptr)[i];
            adv_logger_data->udplogstring_len++;
        }
        
        adv_logger_data->is_new_line = false;
        
        if (((char*) ptr)[i] == '\n') {
            adv_logger_data->is_new_line = true;
        }
    }
    
    if (!adv_logger_data->is_buffered && adv_logger_data->ready_to_send && adv_logger_data->udplogstring_len > 0 && xSemaphoreTake(adv_logger_data->log_sender_semaphore, pdMS_TO_TICKS(10)) == pdTRUE) {
        lwip_sendto(adv_logger_data->socket, adv_logger_data->udplogstring, adv_logger_data->udplogstring_len, 0, adv_logger_data->res->ai_addr, adv_logger_data->res->ai_addrlen);
        free(adv_logger_data->udplogstring);
        adv_logger_data->udplogstring = NULL;
        adv_logger_data->udplogstring_len = 0;
        
        xSemaphoreGive(adv_logger_data->log_sender_semaphore);
    }
    
    return len;
}

static void adv_logger_buffered_task() {
    uint8_t i = 0;
    
    for (;;) {
        i++;
        
        if ((i == 8 && adv_logger_data->udplogstring_len > 0) || adv_logger_data->udplogstring_len > (UDP_LOG_LEN >> 1)) {
            adv_logger_data->ready_to_send = false;
            lwip_sendto(adv_logger_data->socket, adv_logger_data->udplogstring, adv_logger_data->udplogstring_len, 0, adv_logger_data->res->ai_addr, adv_logger_data->res->ai_addrlen);
            adv_logger_data->udplogstring_len = 0;
            adv_logger_data->ready_to_send = true;
            
            i = 0;
        }

        if (i == 8) {
            i = 0;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void adv_logger_init_task(void* args) {
    char* destination = (char*) args;
    
    struct ip_info info;

    while (sdk_wifi_station_get_connect_status() != STATION_GOT_IP) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    if (sdk_wifi_get_ip_info(STATION_IF, &info)) {
        uint8_t macaddr[6];
        sdk_wifi_get_macaddr(STATION_IF, macaddr);
        snprintf(adv_logger_data->header, HEADER_LEN, IPSTR"-%02X%02X%02X", IP2STR(&info.ip), macaddr[3], macaddr[4], macaddr[5]);
        
        const struct addrinfo hints = {
            .ai_family = AF_UNSPEC,
            .ai_socktype = SOCK_DGRAM,
        };
        
        char* dest_port = strchr(destination, ':');
        dest_port[0] = 0;
        dest_port += 1;

        while (getaddrinfo(destination, dest_port, &hints, &adv_logger_data->res) != 0) {
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        
        while (adv_logger_data->socket < 0) {
            adv_logger_data->socket = socket(adv_logger_data->res->ai_family, adv_logger_data->res->ai_socktype, 0);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        
        if (adv_logger_data->is_buffered) {
            adv_logger_data->udplogstring = malloc(UDP_LOG_LEN);
        } else {
            adv_logger_data->udplogstring_size = 63;
            adv_logger_data->udplogstring = malloc(adv_logger_data->udplogstring_size);
        }
        adv_logger_data->udplogstring[0] = 0;
        
        strcat(adv_logger_data->udplogstring, "\r\nAdvanced ESP Logger (c) 2020 José Antonio Jiménez Campos\r\n\r\n");
        adv_logger_data->udplogstring_len = strlen(adv_logger_data->udplogstring);

        if (adv_logger_data->is_buffered) {
            xTaskCreate(adv_logger_buffered_task, "adv_logger_buff", ADV_LOGGER_BUFFERED_TASK_SIZE, NULL, ADV_LOGGER_BUFFERED_TASK_PRIORITY, &adv_logger_data->xHandle);
        }
        
        adv_logger_data->is_new_line = true;
        adv_logger_data->ready_to_send = true;

    } else {
        adv_logger_remove();
    }
    
    free(args);
    
    vTaskDelete(NULL);
}

int adv_logger_remove() {
    if (adv_logger_original_write_function) {
        set_write_stdout(adv_logger_original_write_function);
        adv_logger_original_write_function = NULL;
    
        if (adv_logger_data) {
            if (adv_logger_data->header) {
                free(adv_logger_data->header);
            }
            
            if (adv_logger_data->udplogstring) {
                free(adv_logger_data->udplogstring);
            }
            
            free(adv_logger_data);
        }
        
        return 0;
    }
    
    return -1;
}

void adv_logger_init(const uint8_t log_type, char* dest_addr) {
    adv_logger_remove();
    
    adv_logger_original_write_function = get_write_stdout();
    
    if (log_type == ADV_LOGGER_NONE) {
        set_write_stdout(adv_logger_none);

    } else {
        adv_logger_data = malloc(sizeof(adv_logger_data_t));
        memset(adv_logger_data, 0, sizeof(*adv_logger_data));
        
        adv_logger_data->log_type = log_type - ADV_LOGGER_UART0;

        if (adv_logger_data->log_type > ADV_LOGGER_UART0_UDP) {
            adv_logger_data->log_type -= 3;
            adv_logger_data->is_buffered = true;
        }
        
        if (adv_logger_data->log_type > ADV_LOGGER_UART0) {
            adv_logger_data->log_type -= 3;         // Can be -1, meaning no UART output
        }

        if (log_type > ADV_LOGGER_UART1) {
            adv_logger_data->socket = -1;
            adv_logger_data->header = malloc(HEADER_LEN);
            if (!adv_logger_data->is_buffered) {
                adv_logger_data->log_sender_semaphore = xSemaphoreCreateMutex();
            }
            
            char* destination;
            
            if (dest_addr != NULL) {
                destination = strdup(dest_addr);
            } else {
                destination = malloc(strlen(ADV_LOGGER_DEFAULT_DESTINATION) + 1);
                snprintf(destination, strlen(ADV_LOGGER_DEFAULT_DESTINATION) + 1, ADV_LOGGER_DEFAULT_DESTINATION);
            }
            
            xTaskCreate(adv_logger_init_task, "adv_logger_init", ADV_LOGGER_INIT_TASK_SIZE, (void*) destination, ADV_LOGGER_INIT_TASK_PRIORITY, NULL);
        }
        
        set_write_stdout(adv_logger_write);
    }
}
