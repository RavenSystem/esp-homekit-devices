/*
 * RavenSystem NTP
 *
 * Copyright 2021-2022 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#include <string.h>

#ifdef ESP_PLATFORM

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_timer.h"

#define sdk_system_get_time_raw()         esp_timer_get_time()

#else

#include <espressif/esp_common.h>
#include <esp8266.h>
#include <esplibs/libmain.h>
#include <FreeRTOS.h>

#endif

#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>
#include <lwip/dns.h>

#include "raven_ntp.h"

#define NTP_TIME_DIFF_1900_1970             (2208988800UL)
#define NTP_PORT                            "123"

typedef struct _raven_ntp_config {
    time_t time;
#ifdef ESP_PLATFORM
    uint64_t last_system_time;
#else
    uint32_t last_system_time;
#endif
} raven_ntp_config_t;

static raven_ntp_config_t* raven_ntp_config = NULL;

static void raven_ntp_init() {
    if (!raven_ntp_config) {
        raven_ntp_config = malloc(sizeof(raven_ntp_config_t));
        memset(raven_ntp_config, 0, sizeof(*raven_ntp_config));
    }
}

time_t raven_ntp_get_time_t() {
    raven_ntp_init();
#ifdef ESP_PLATFORM
    const uint64_t now = sdk_system_get_time_raw();
    const uint64_t diff = (now - raven_ntp_config->last_system_time) / 1000000;
#else
    const uint32_t now = sdk_system_get_time_raw();
    const uint32_t diff = (now - raven_ntp_config->last_system_time) / 1000000;

    if (diff > 3550) {
        raven_ntp_config->last_system_time = now;
        raven_ntp_config->time += diff;
        
        return raven_ntp_config->time;
    }
#endif
    
    return raven_ntp_config->time + diff;
}

void raven_ntp_get_log_time(char* buffer, const size_t buffer_size) {
    struct tm* timeinfo;
    time_t utc_time = raven_ntp_get_time_t();
    timeinfo = localtime(&utc_time);
    
    char month[4];
    month[0] = 0;
    switch (timeinfo->tm_mon) {
        case 0:
            strcat(month, "Jan");
            break;
            
        case 1:
            strcat(month, "Feb");
            break;
            
        case 2:
            strcat(month, "Mar");
            break;
            
        case 3:
            strcat(month, "Apr");
            break;
            
        case 4:
            strcat(month, "May");
            break;
            
        case 5:
            strcat(month, "Jun");
            break;
            
        case 6:
            strcat(month, "Jul");
            break;
            
        case 7:
            strcat(month, "Aug");
            break;
            
        case 8:
            strcat(month, "Sep");
            break;
            
        case 9:
            strcat(month, "Oct");
            break;
            
        case 10:
            strcat(month, "Nov");
            break;
            
        case 11:
            strcat(month, "Dec");
            break;
    }

    snprintf(buffer, buffer_size, "%s %.2d %.2d:%.2d:%.2d", month, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
}

/*
 Error codes:
  0: Updated OK
 -1: NTP Bad response
 -2: NTP Error Send
 -3: NTP Payload no memory
 -4: NTP Error creating Socket
 -5: NTP DNS error
 */
int raven_ntp_update(char* ntp_server) {
    raven_ntp_get_time_t();
    
    int ntp_result = -5;
    
    struct addrinfo* res;
    const struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_DGRAM,
    };
    
    int getaddr_result = getaddrinfo(ntp_server, NTP_PORT, &hints, &res);
    if (getaddr_result == 0) {
        int s = socket(res->ai_family, res->ai_socktype, 0);
        if (s >= 0) {
            uint8_t* ntp_payload = malloc(49);
            if (ntp_payload) {
                memset(ntp_payload, 0, 49);
                ntp_payload[0] = 0x1B;
                
                const struct timeval sndtimeout = { 3, 0 };
                setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &sndtimeout, sizeof(sndtimeout));
                
                const struct timeval rcvtimeout = { 2, 0 };
                setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &rcvtimeout, sizeof(rcvtimeout));
                
                int result = lwip_sendto(s, ntp_payload, 48, 0, res->ai_addr, res->ai_addrlen);
                
                if (result > 0) {
                    memset(ntp_payload, 0, 49);
                    int read_byte = lwip_read(s, ntp_payload, 49);
                    
                    if (read_byte == 48) {
                        uint32_t recv_time = 0;
                        memcpy(&recv_time, &ntp_payload[32], sizeof(recv_time));
                        
                        raven_ntp_config->last_system_time = sdk_system_get_time_raw();
                        raven_ntp_config->time = (ntohl(recv_time) - NTP_TIME_DIFF_1900_1970);
                        
                        ntp_result = 0;
                    } else {
                        ntp_result = -1;
                    }

                } else {
                    ntp_result = -2;
                }

                free(ntp_payload);
                
            } else {
                ntp_result = -3;
            }
            
        } else {
            ntp_result = -4;
        }
    
        lwip_close(s);
    }
    
    freeaddrinfo(res);
    
    return ntp_result;
}
