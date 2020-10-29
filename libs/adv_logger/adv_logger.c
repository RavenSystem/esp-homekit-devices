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
#include <esplibs/libmain.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <string.h>
#include <lwip/sockets.h>
#include <stdout_redirect.h>
#include <semphr.h>

#define HEADER_LEN                          (23)
#define UDP_LOG_LEN                         (1472)

// Task Stack Size                          configMINIMAL_STACK_SIZE = 256
#define ADV_LOGGER_INIT_TASK_SIZE           (configMINIMAL_STACK_SIZE)
#define ADV_LOGGER_BUFFERED_TASK_SIZE       (configMINIMAL_STACK_SIZE)

// Task Priority
#define ADV_LOGGER_INIT_TASK_PRIORITY       (tskIDLE_PRIORITY + 0)
#define ADV_LOGGER_BUFFERED_TASK_PRIORITY   (configMAX_PRIORITIES)

#define DESTINATION_PORT                (28338)     // 45678 in reversed bytes
#define SOURCE_PORT                     (40109)     // 44444 in reversed bytes

typedef struct _adv_logger_data {
    int lSocket;
    
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
    
    struct sockaddr_in sLocalAddr, sDestAddr;
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
        
        if (adv_logger_data->log_type >= 0) {
            uart_putc(adv_logger_data->log_type, ((char*) ptr)[i]);
        }
        
        if (adv_logger_data->ready_to_send && is_adv_logger_data) {
            if (adv_logger_data->is_new_line) {
                adv_logger_data->is_new_line = false;
                
                if (!adv_logger_data->is_buffered) {
                    adv_logger_data->udplogstring_size += 35;
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
                char buffer[10];
                snprintf(buffer, 10, "%08.3f ", (float) sdk_system_get_time() * 1e-6);
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
    
    if (!adv_logger_data->is_buffered && adv_logger_data->ready_to_send && adv_logger_data->udplogstring_len > 0 && xSemaphoreTake(adv_logger_data->log_sender_semaphore, pdMS_TO_TICKS(10)) == pdTRUE) {
        lwip_sendto(adv_logger_data->lSocket, adv_logger_data->udplogstring, adv_logger_data->udplogstring_len, 0, (struct sockaddr*) &adv_logger_data->sDestAddr, sizeof(adv_logger_data->sDestAddr));
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
        
        if ((i == 5 && adv_logger_data->udplogstring_len > 0) || adv_logger_data->udplogstring_len > (UDP_LOG_LEN >> 1)) {
            adv_logger_data->ready_to_send = false;
            lwip_sendto(adv_logger_data->lSocket, adv_logger_data->udplogstring, adv_logger_data->udplogstring_len, 0, (struct sockaddr*) &adv_logger_data->sDestAddr, sizeof(adv_logger_data->sDestAddr));
            adv_logger_data->udplogstring_len = 0;
            adv_logger_data->ready_to_send = true;
            
            i = 0;
        }

        if (i == 5) {
            i = 0;
        }
        
        vTaskDelay(pdMS_TO_TICKS(20));
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
        
        adv_logger_data->lSocket = lwip_socket(AF_INET, SOCK_DGRAM, 0);
        memset((char*) &adv_logger_data->sLocalAddr, 0, sizeof(adv_logger_data->sLocalAddr));
        memset((char*) &adv_logger_data->sDestAddr, 0, sizeof(adv_logger_data->sDestAddr));
        
        // Source
        adv_logger_data->sLocalAddr.sin_family = AF_INET;
        adv_logger_data->sLocalAddr.sin_len = sizeof(adv_logger_data->sLocalAddr);
        adv_logger_data->sLocalAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        adv_logger_data->sLocalAddr.sin_port = SOURCE_PORT;
        lwip_bind(adv_logger_data->lSocket, (struct sockaddr*) &adv_logger_data->sLocalAddr, sizeof(adv_logger_data->sLocalAddr));
        
        // Destination
        adv_logger_data->sDestAddr.sin_family = AF_INET;
        adv_logger_data->sDestAddr.sin_len = sizeof(adv_logger_data->sDestAddr);
        adv_logger_data->sDestAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
        adv_logger_data->sDestAddr.sin_port = DESTINATION_PORT;
        
        if (adv_logger_data->is_buffered) {
            adv_logger_data->udplogstring = malloc(UDP_LOG_LEN);
        } else {
            adv_logger_data->udplogstring_size = 63;
            adv_logger_data->udplogstring = malloc(adv_logger_data->udplogstring_size);
        }
        if (adv_logger_data->udplogstring == NULL) {
            return;
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

void adv_logger_init(const uint8_t log_type) {
    adv_logger_remove();
    
    adv_logger_original_write_function = get_write_stdout();
    
    if (log_type == ADV_LOGGER_NONE) {
        set_write_stdout(adv_logger_none);

    } else if (log_type > ADV_LOGGER_UART0) {
        adv_logger_data = malloc(sizeof(adv_logger_data_t));
        if (adv_logger_data == NULL) {
            return;
        }
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
            adv_logger_data->header = malloc(HEADER_LEN);
            if (!adv_logger_data->is_buffered) {
                adv_logger_data->log_sender_semaphore = xSemaphoreCreateMutex();
            }
            
            xTaskCreate(adv_logger_init_task, "adv_logger_init", ADV_LOGGER_INIT_TASK_SIZE, NULL, ADV_LOGGER_INIT_TASK_PRIORITY, NULL);
        }
        
        set_write_stdout(adv_logger_write);
    }
}
