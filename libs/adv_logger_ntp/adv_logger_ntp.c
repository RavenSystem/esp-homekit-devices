/*
 * Advanced ESP Logger with NTP support
 *
 * Copyright 2020-2022 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#include <stdio.h>
#include <string.h>

#ifdef ESP_PLATFORM

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "esp_attr.h"

#define sdk_system_get_time_raw()           ((uint32_t) esp_timer_get_time())

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

#include <raven_ntp.h>

#include "adv_logger.h"

#define HEADER_LEN                          (23)
#define UDP_LOG_LEN                         (1400)

// Task Stack Size
#ifdef ESP_PLATFORM
#define ADV_LOGGER_INIT_TASK_SIZE           (2048)
#define ADV_LOGGER_BUFFERED_TASK_SIZE       (2048)
#define ADV_LOGGER_VSNPRINTF_BUFFER_SIZE    (1024)
#else
#define ADV_LOGGER_INIT_TASK_SIZE           (configMINIMAL_STACK_SIZE)  // 256
#define ADV_LOGGER_BUFFERED_TASK_SIZE       (configMINIMAL_STACK_SIZE)  // 256
#endif

// Task Priority
#define ADV_LOGGER_INIT_TASK_PRIORITY       (tskIDLE_PRIORITY + 0)
#define ADV_LOGGER_BUFFERED_TASK_PRIORITY   (configMAX_PRIORITIES - 1)

#define ADV_LOGGER_DEFAULT_DESTINATION      "255.255.255.255:45678"

typedef struct _adv_logger_data {
    int32_t socket;
    
#ifdef ESP_PLATFORM
    char* ip_address;
    SemaphoreHandle_t log_sender_semaphore;
#endif
    
    uint16_t udplogstring_len: 11;
    int8_t log_type: 4;
    bool with_header: 1;
    
    uint16_t udplogstring_size: 11;
    bool is_new_line: 1;
    bool ready_to_send: 1;
    bool is_buffered: 1;
    
    TaskHandle_t xHandle;
    
    char* udplogstring;
    char* header;
    
    struct addrinfo* res;
} adv_logger_data_t;

static adv_logger_data_t* adv_logger_data = NULL;

#ifdef ADV_LOGGER_REMOVE
#ifndef ESP_PLATFORM
static _WriteFunction* adv_logger_original_write_function = NULL;
#endif
#endif

static ssize_t adv_logger_write(struct _reent* r, int fd, const void* ptr, size_t len) {
#ifdef ESP_PLATFORM
    if (xSemaphoreTake(adv_logger_data->log_sender_semaphore, 0) == pdTRUE) {
#endif
        int is_adv_logger_data = false;
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
        
        char* buffer = NULL;
        
        if (adv_logger_data->with_header) {
            buffer = malloc(17);
        }
        
        for (size_t i = 0; i < len; i++) {
            // Auto convert CR to CRLF, ignore other LFs (compatible with Espressif SDK behaviour)
            if (((char*) ptr)[i] == '\r') {
                continue;
            }
            
            if (adv_logger_data->udplogstring_len > (UDP_LOG_LEN - 40 - 1)) {
                is_adv_logger_data = false;
            }
            
            if (((char*) ptr)[i] == '\n') {
                if (adv_logger_data->log_type >= 0) {
#ifdef ESP_PLATFORM
                    char new_line_char = '\r';
                    uart_write_bytes(adv_logger_data->log_type, &new_line_char, 1);
#else
                    uart_putc(adv_logger_data->log_type, '\r');
#endif
                }
                
                if (adv_logger_data->ready_to_send && is_adv_logger_data) {
                    adv_logger_data->udplogstring[adv_logger_data->udplogstring_len] = '\r';
                    adv_logger_data->udplogstring_len++;
                }
            }
            
            if (adv_logger_data->with_header && adv_logger_data->is_new_line) {
                raven_ntp_get_log_time(buffer, 16);
                buffer[15] = ' ';
                buffer[16] = 0;
            }
            
            if (adv_logger_data->log_type >= 0) {
                if (adv_logger_data->with_header && adv_logger_data->is_new_line) {
                    for (int j = 0; j < 16; j++) {
#ifdef ESP_PLATFORM
                        uart_write_bytes(adv_logger_data->log_type, &buffer[j], 1);
#else
                        uart_putc(adv_logger_data->log_type, buffer[j]);
#endif
                    }
                }
                
#ifdef ESP_PLATFORM
                uart_write_bytes(adv_logger_data->log_type, &((char*) ptr)[i], 1);
#else
                uart_putc(adv_logger_data->log_type, ((char*) ptr)[i]);
#endif
            }
            
            if (adv_logger_data->ready_to_send && is_adv_logger_data) {
                if (adv_logger_data->is_new_line) {
                    if (!adv_logger_data->is_buffered) {
                        if (adv_logger_data->with_header) {
                            adv_logger_data->udplogstring_size += 40;
                        }
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
                    if (adv_logger_data->with_header) {
                        strcat(strcat(strcat(adv_logger_data->udplogstring, buffer), adv_logger_data->header), " ");
                    }
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
        
        if (buffer) {
            free(buffer);
        }
        
        if (!adv_logger_data->is_buffered && adv_logger_data->ready_to_send && adv_logger_data->udplogstring_len > 0) {
            lwip_sendto(adv_logger_data->socket, adv_logger_data->udplogstring, adv_logger_data->udplogstring_len, 0, adv_logger_data->res->ai_addr, adv_logger_data->res->ai_addrlen);
            free(adv_logger_data->udplogstring);
            adv_logger_data->udplogstring = NULL;
            adv_logger_data->udplogstring_len = 0;
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
        
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

static void adv_logger_init_task(void* args) {
    char* destination = (char*) args;
    
#ifdef ESP_PLATFORM
    while (!adv_logger_data->ip_address) {
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
#else
    while (sdk_wifi_station_get_connect_status() != STATION_GOT_IP) {
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
#endif
    
    if (adv_logger_data->with_header) {
        uint8_t macaddr[6];
#ifdef ESP_PLATFORM
        esp_wifi_get_mac(WIFI_IF_STA, macaddr);
        snprintf(adv_logger_data->header, HEADER_LEN, "%s-%02X%02X%02X", adv_logger_data->ip_address, macaddr[3], macaddr[4], macaddr[5]);
#else
        struct ip_info info;
        sdk_wifi_get_ip_info(STATION_IF, &info);
        sdk_wifi_get_macaddr(STATION_IF, macaddr);
        snprintf(adv_logger_data->header, HEADER_LEN, IPSTR"-%02X%02X%02X", IP2STR(&info.ip), macaddr[3], macaddr[4], macaddr[5]);
#endif
    }
    
#ifdef ESP_PLATFORM
    free(adv_logger_data->ip_address);
#endif
        
    const struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_DGRAM,
    };
    
    char* dest_port = strchr(destination, ':');
    dest_port[0] = 0;
    dest_port += 1;
    
    while (getaddrinfo(destination, dest_port, &hints, &adv_logger_data->res) != 0) {
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
    
    free(args);     // destination and port
    
    while ((adv_logger_data->socket = lwip_socket(adv_logger_data->res->ai_family, adv_logger_data->res->ai_socktype, 0)) < 0) {
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
    
    if (adv_logger_data->is_buffered) {
        adv_logger_data->udplogstring = malloc(UDP_LOG_LEN);
    } else {
        adv_logger_data->udplogstring_size = 63;
        adv_logger_data->udplogstring = malloc(adv_logger_data->udplogstring_size);
    }
    adv_logger_data->udplogstring[0] = 0;
    
    strcat(adv_logger_data->udplogstring, "\r\nAdv Log (c) 2022-2023 José A. Jiménez Campos\r\n\r\n");
    adv_logger_data->udplogstring_len = strlen(adv_logger_data->udplogstring);
    
    if (adv_logger_data->is_buffered) {
        xTaskCreate(adv_logger_buffered_task, "LOG", ADV_LOGGER_BUFFERED_TASK_SIZE, NULL, ADV_LOGGER_BUFFERED_TASK_PRIORITY, &adv_logger_data->xHandle);
    }
    
    adv_logger_data->is_new_line = true;
    adv_logger_data->ready_to_send = true;
    
    vTaskDelete(NULL);
}

#ifdef ESP_PLATFORM
void adv_logger_set_ip_address(char* ip_address) {
    if (adv_logger_data) {
        adv_logger_data->ip_address = ip_address;
    } else {
        free(ip_address);
    }
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
#else
static ssize_t IRAM adv_logger_none(struct _reent* r, int fd, const void* ptr, size_t len) {
    return len;
}
#endif

#ifdef ADV_LOGGER_REMOVE
#ifndef ESP_PLATFORM
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
#endif
#endif

void adv_logger_init(const uint8_t log_type, char* dest_addr, const bool with_header) {
#ifdef ADV_LOGGER_REMOVE
#ifndef ESP_PLATFORM
    adv_logger_remove();
    
    adv_logger_original_write_function = get_write_stdout();
#endif
#endif
    
    if (log_type == ADV_LOGGER_NONE) {
#ifndef ESP_PLATFORM
        set_write_stdout(adv_logger_none);
#endif
        
    } else {
        adv_logger_data = malloc(sizeof(adv_logger_data_t));
        memset(adv_logger_data, 0, sizeof(*adv_logger_data));
        
        adv_logger_data->log_type = (log_type % 4) - 1;
        adv_logger_data->with_header = with_header;
        
#ifdef ESP_PLATFORM
        adv_logger_data->log_sender_semaphore = xSemaphoreCreateMutex();
#endif
        
        if (log_type >= ADV_LOGGER_UDP) {
            if (log_type >= ADV_LOGGER_UDP_BUFFERED) {
                adv_logger_data->is_buffered = true;
            }
            
            adv_logger_data->socket = -1;
            if (with_header) {
                adv_logger_data->header = malloc(HEADER_LEN);
            }
            
            char* destination;
            
            if (dest_addr != NULL) {
                destination = strdup(dest_addr);
            } else {
                destination = malloc(strlen(ADV_LOGGER_DEFAULT_DESTINATION) + 1);
                snprintf(destination, strlen(ADV_LOGGER_DEFAULT_DESTINATION) + 1, ADV_LOGGER_DEFAULT_DESTINATION);
            }
            
            xTaskCreate(adv_logger_init_task, "LOG", ADV_LOGGER_INIT_TASK_SIZE, (void*) destination, ADV_LOGGER_INIT_TASK_PRIORITY, NULL);
        }
        
#ifndef ESP_PLATFORM
        set_write_stdout(adv_logger_write);
#endif
    }
}
