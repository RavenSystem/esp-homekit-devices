/*
 * Home Accessory Architect OTA Update
 *
 * Copyright 2020-2024 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

/*
 * Based on esp-wifi-config library by Maxim Kulkin (@MaximKulkin), licensed under the MIT License.
 * https://github.com/maximkulkin/esp-wifi-config
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "header.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "errno.h"
#include "esp32_port.h"
#include "http_parser.h"

#define spiflash_read(address, buffer, length)      esp_flash_read(NULL, buffer, address, length)
#define spiflash_write(address, buffer, length)     esp_flash_write(NULL, buffer, address, length)
#define spiflash_erase_sector(address)              esp_flash_erase_region(NULL, address, SPI_FLASH_SECTOR_SIZE)

#if defined(CONFIG_IDF_TARGET_ESP32) \
    || defined(CONFIG_IDF_TARGET_ESP32S2)
#define SPIFLASHMODE_OFFSET                         (0x1000)
#else
#define SPIFLASHMODE_OFFSET                         (0x0)
#endif


#if defined(HAA_SHELLY_Plus1)
#define HAA_SHELLY_INITIAL_SCRIPT   "{\"c\":{\"io\":[[[26,0],2],[[25],6,1],[[4],6,0,1],[[32],10,0,3]],\"l\":0,\"b\":[[25,5]]},\"a\":[{\"0\":{\"r\":[[26]]},\"1\":{\"r\":[[26,1]]},\"b\":[[4],[4,0]],\"es\":[{\"t\":22,\"h\":2,\"n\":5,\"g\":32,\"j\":10,\"y0\":[{\"v\":75,\"r\":1,\"0\":{\"m\":[[1]]}}]}]}]}"

#elif defined(HAA_SHELLY_S1G3)
#define HAA_SHELLY_INITIAL_SCRIPT   "{\"c\":{\"io\":[[[5,19],2],[[1],6,1],[[10],6,0,1],[[3],10,0,3]],\"l\":19,\"b\":[[1,5]]},\"a\":[{\"0\":{\"r\":[[5]]},\"1\":{\"r\":[[5,1]]},\"b\":[[10],[10,0]],\"es\":[{\"t\":22,\"h\":2,\"n\":5,\"g\":3,\"j\":10,\"y0\":[{\"v\":75,\"r\":1,\"0\":{\"m\":[[1]]}}]}]}]}"

#elif defined(HAA_SHELLY_Plus1PM)
#define HAA_SHELLY_INITIAL_SCRIPT   "{\"c\":{\"io\":[[[26,0,23],2],[[25],6,1],[[4],6,0,1],[[5,18],1],[[32],10,0,3]],\"l\":0,\"b\":[[25,5]]},\"a\":[{\"0\":{\"r\":[[26]]},\"1\":{\"r\":[[26,1]]},\"b\":[[4],[4,0]],\"es\":[{\"t\":75,\"dt\":[5,18,23],\"vf\":0.140637,\"cf\":0.011041,\"pf\":1.632888},{\"t\":22,\"h\":2,\"n\":5,\"g\":32,\"j\":10,\"y0\":[{\"v\":75,\"r\":1,\"0\":{\"m\":[[1]]}}]}]}]}"

#elif defined(HAA_SHELLY_Plus2PM)
#define HAA_SHELLY_INITIAL_SCRIPT   "{\"c\":{\"io\":[[[12,13,0],2],[[4],6,1],[[5,18],6,0,1],[[35],10,0,3],[[27]]],\"l\":0,\"b\":[[4,5]],\"ic\":[[25,26,100]]},\"a\":[{\"0\":{\"r\":[[13]]},\"1\":{\"r\":[[13,1]]},\"b\":[[5],[5,0]],\"es\":[{\"0\":{\"r\":[[12]]},\"1\":{\"r\":[[12,1]]},\"b\":[[18],[18,0]]},{\"t\":75,\"cd\":1.5,\"n\":2,\"dt\":[0,56],\"vf\":0.0000382602,\"vo\":-0.068,\"cf\":0.00000949523,\"co\":-0.017,\"pf\":0.006097560976},{\"t\":75,\"n\":3,\"dt\":[0,56],\"vf\":0.0000382602,\"vo\":-0.068,\"cf\":0.00000949523,\"co\":-0.017,\"pf\":0.006097560976},{\"t\":22,\"h\":2,\"n\":5,\"g\":35,\"j\":10,\"y0\":[{\"v\":75,\"r\":1,\"0\":{\"m\":[[1],[2]]}}]}]}]}"

#elif defined(HAA_SHELLY_S2PMG3)
#define HAA_SHELLY_INITIAL_SCRIPT   "{\"c\":{\"io\":[[[5,3,2],2],[[19],6,1],[[18,10],6,0,1],[[1],10,0,3],[[4]]],\"l\":2,\"b\":[[19,5]],\"ic\":[[7,6,100]]},\"a\":[{\"0\":{\"r\":[[5]]},\"1\":{\"r\":[[5,1]]},\"b\":[[18],[18,0]],\"es\":[{\"0\":{\"r\":[[3]]},\"1\":{\"r\":[[3,1]]},\"b\":[[10],[10,0]]},{\"t\":75,\"cd\":1.5,\"n\":2,\"dt\":[0,56],\"vf\":0.0000382602,\"vo\":-0.068,\"cf\":0.00000949523,\"co\":-0.017,\"pf\":0.006097560976},{\"t\":75,\"n\":3,\"dt\":[0,56],\"vf\":0.0000382602,\"vo\":-0.068,\"cf\":0.00000949523,\"co\":-0.017,\"pf\":0.006097560976},{\"t\":22,\"h\":2,\"n\":5,\"g\":1,\"j\":10,\"y0\":[{\"v\":75,\"r\":1,\"0\":{\"m\":[[1],[2]]}}]}]}]}"

#elif defined(HAA_SHELLY_PlusPlugS) \
    || defined(HAA_SHELLY_PlusPlugUK) \
    || defined(HAA_SHELLY_PlusPlugIT) \
    || defined(HAA_SHELLY_PlugUS)
#define HAA_SHELLY_INITIAL_SCRIPT   "{\"c\":{\"io\":[[[4,19,25,26],2],[[9],6],[[10,22],1],[[33],10,0,3]],\"l\":25,\"b\":[[9,5]]},\"a\":[{\"t\":2,\"0\":{\"r\":[[4],[26,1]]},\"1\":{\"r\":[[4,1],[26]]},\"b\":[[9]],\"es\":[{\"t\":75,\"dt\":[10,22,19],\"vf\":0.15892,\"cf\":0.012477,\"pf\":1.845164},{\"t\":22,\"h\":2,\"n\":5,\"g\":33,\"j\":10,\"y0\":[{\"v\":75,\"r\":1,\"0\":{\"m\":[[1]]}}]}]}]}"

#elif defined(HAA_SHELLY_PlusI4)
#define HAA_SHELLY_INITIAL_SCRIPT   "{\"c\":{\"io\":[[[12,14,27,26],6,0,1]]},\"a\":[{\"t\":3,\"f0\":[[12]],\"f1\":[[12,2]],\"f2\":[[12,3]]},{\"t\":3,\"f0\":[[14]],\"f1\":[[14,2]],\"f2\":[[14,3]]},{\"t\":3,\"f0\":[[27]],\"f1\":[[27,2]],\"f2\":[[27,3]]},{\"t\":3,\"f0\":[[26]],\"f1\":[[26,2]],\"f2\":[[26,3]]}]}"

#elif defined(HAA_SHELLY_I4G3)
#define HAA_SHELLY_INITIAL_SCRIPT   "{\"c\":{\"io\":[[[10,7,6,5],6,0,1],[[3],10,0,3]]},\"a\":[{\"t\":3,\"f0\":[[10]],\"f1\":[[10,2]],\"f2\":[[10,3]],\"es\":[{\"t\":22,\"h\":2,\"n\":5,\"g\":3,\"j\":10}]},{\"t\":3,\"f0\":[[7]],\"f1\":[[7,2]],\"f2\":[[7,3]]},{\"t\":3,\"f0\":[[6]],\"f1\":[[6,2]],\"f2\":[[6,3]]},{\"t\":3,\"f0\":[[5]],\"f1\":[[5,2]],\"f2\":[[5,3]]}]}"

#elif defined(HAA_SHELLY_PlusUni)
#define HAA_SHELLY_INITIAL_SCRIPT   "{\"c\":{\"io\":[[[19,21,18],2],[[37,38],6],[[36],10,0,3]],\"l\":18},\"a\":[{\"0\":{\"r\":[[19]]},\"1\":{\"r\":[[19,1]]},\"b\":[[38]],\"es\":[{\"0\":{\"r\":[[21]]},\"1\":{\"r\":[[21,1]]},\"b\":[[37]]},{\"t\":80,\"j\":5,\"n\":10,\"g\":[36]},{\"t\":24,\"g\":13}]}]}"

#elif defined(HAA_SHELLY_PlusRGBWPM)
#define HAA_SHELLY_INITIAL_SCRIPT   "{\"c\":{\"io\":[[[25,26],9],[[27,4],9,0,0,2],[[0],2],[[36,37,38,39],6,0,1],[[33,34,35]]],\"l\":0},\"a\":[{\"t\":30,\"g\":[25,26,27,4],\"b\":[[36],[37],[38],[38,0],[39],[39,0]]}]}"

#elif defined(HAA_SHELLY_Plus10V)
#define HAA_SHELLY_INITIAL_SCRIPT   "{\"c\":{\"io\":[[[26],9,0,65535,1],[[25],6,1],[[4,18],6,0,1],[[32],10,0,3]],\"l\":0,\"b\":[[25,5]]},\"a\":[{\"t\":30,\"g\":[26],\"b\":[[4]]},{\"t\":3,\"f0\":[[18]],\"f1\":[[18,2]],\"f2\":[[18,3]],\"es\":[{\"t\":22,\"h\":2,\"n\":5,\"g\":32,\"j\":10,\"y0\":[{\"v\":75,\"r\":1,\"0\":{\"m\":[[1]]}}]}]}]}"

#elif defined(HAA_SHELLY_Plus1Mini) \
    || defined(HAA_SHELLY_Mini1G3)
#define HAA_SHELLY_INITIAL_SCRIPT   "{\"c\":{\"io\":[[[7,0],2],[[1],6,1],[[10],6,0,1],[[3],10,0,3]],\"l\":0,\"b\":[[1,5]]},\"a\":[{\"0\":{\"r\":[[7]]},\"1\":{\"r\":[[7,1]]},\"b\":[[10],[10,0],[1]],\"es\":[{\"t\":22,\"h\":2,\"n\":5,\"g\":3,\"j\":10,\"y0\":[{\"v\":75,\"r\":1,\"0\":{\"m\":[[1]]}}]}]}]}"

#elif defined(HAA_SHELLY_Plus1PMMini) \
    || defined(HAA_SHELLY_Mini1PMG3)
#define HAA_SHELLY_INITIAL_SCRIPT   "{\"c\":{\"io\":[[[5,0],2],[[1],6,1],[[10],6,0,1],[[3],10,0,3]],\"l\":0,\"b\":[[1,5]],\"r\":[{\"n\":10,\"g\":[6,7],\"s\":9600,\"l\":[22,22]}]},\"a\":[{\"0\":{\"r\":[[5]]},\"1\":{\"r\":[[5,1]]},\"b\":[[10],[10,0],[1]],\"es\":[{\"t\":75,\"n\":4},{\"t\":22,\"h\":2,\"n\":5,\"g\":3,\"j\":10,\"y0\":[{\"v\":75,\"r\":1,\"0\":{\"m\":[[1]]}}]}]},{\"t\":80,\"h\":0,\"tg\":[2,1],\"n\":24,\"ff\":0.000003980678349,\"pt\":[[\"55\"]],\"dt\":[3,1]},{\"t\":80,\"h\":0,\"tg\":[2,0],\"n\":24,\"ff\":0.0000629986364,\"pt\":[[\"55\",3]],\"dt\":[3,1]},{\"t\":80,\"h\":0,\"tg\":[2,2],\"n\":24,\"ff\":0.001677852349,\"pt\":[[\"55\",9]],\"dt\":[3,1]},{\"h\":0,\"i\":5,\"s\":1,\"0\":{\"u\":[{\"v\":\"58aa\"}],\"m\":[[0,1]]}}]}"

#elif defined(HAA_SHELLY_PlusPMMini) \
    || defined(HAA_SHELLY_MiniPMG3)
#define HAA_SHELLY_INITIAL_SCRIPT   "{\"c\":{\"io\":[[[0],2],[[1],6,1],[[3],10,0,3]],\"l\":0,\"b\":[[1,5]],\"r\":[{\"n\":10,\"g\":[6,7],\"s\":9600,\"l\":[22,22]}]},\"a\":[{\"t\":22,\"n\":5,\"g\":3,\"j\":10,\"es\":[{\"t\":75,\"n\":4}]},{\"t\":80,\"h\":0,\"tg\":[2,1],\"n\":24,\"ff\":0.000003980678349,\"pt\":[[\"55\"]],\"dt\":[3,1]},{\"t\":80,\"h\":0,\"tg\":[2,0],\"n\":24,\"ff\":0.0000629986364,\"pt\":[[\"55\",3]],\"dt\":[3,1]},{\"t\":80,\"h\":0,\"tg\":[2,2],\"n\":24,\"ff\":0.001677852349,\"pt\":[[\"55\",9]],\"dt\":[3,1]},{\"h\":0,\"i\":5,\"s\":1,\"0\":{\"u\":[{\"v\":\"58aa\"}],\"m\":[[0,1]]}}]}"

#elif defined(HAA_SHELLY_Pro1)
#define HAA_SHELLY_INITIAL_SCRIPT   "{\"c\":{\"mc\":[[114,13,4,8,8]],\"io\":[[[35],6,1],[[38,39],6,0,1],[[36],10,0,3]],\"l\":105,\"b\":[[35,5]]},\"a\":[{\"0\":{\"r\":[[107],[106]]},\"1\":{\"r\":[[107,1],[106,1]]},\"b\":[[38],[38,0],[39],[35]],\"es\":[{\"t\":22,\"h\":2,\"n\":5,\"g\":36,\"j\":10,\"y0\":[{\"v\":75,\"r\":1,\"0\":{\"m\":[[1]]}}]}]}]}"

#elif defined(HAA_SHELLY_Pro2)
#define HAA_SHELLY_INITIAL_SCRIPT   "{\"c\":{\"mc\":[[114,13,4,8,8]],\"io\":[[[35],6,1],[[38,39],6,0,1],[[36],10,0,3]],\"l\":105,\"b\":[[35,5]]},\"a\":[{\"0\":{\"r\":[[107]]},\"1\":{\"r\":[[107,1]]},\"b\":[[38],[38,0]]},{\"0\":{\"r\":[[106]]},\"1\":{\"r\":[[106,1]]},\"b\":[[39],[39,0]],\"es\":[{\"t\":22,\"h\":2,\"n\":5,\"g\":36,\"j\":10,\"y0\":[{\"v\":75,\"r\":1,\"0\":{\"m\":[[1],[2]]}}]}]}]}"

#elif defined(HAA_SHELLY_Pro3)
#define HAA_SHELLY_INITIAL_SCRIPT   "{\"c\":{\"io\":[[[2,4,32,12],2],[[13,14],2,0,1],[[35],6,1],[[34,37,38],6,0,1],[[36],10,0,3]],\"l\":13,\"b\":[[35,5]]},\"a\":[{\"0\":{\"r\":[[2]]},\"1\":{\"r\":[[2,1]]},\"b\":[[34],[34,0]]},{\"0\":{\"r\":[[4]]},\"1\":{\"r\":[[4,1]]},\"b\":[[37],[37,0]]},{\"0\":{\"r\":[[32]]},\"1\":{\"r\":[[32,1]]},\"b\":[[38],[38,0]],\"es\":[{\"t\":22,\"h\":2,\"n\":5,\"g\":36,\"j\":10,\"y0\":[{\"v\":75,\"r\":1,\"0\":{\"m\":[[1],[2],[3]]}}]}]}]}"

#endif

#include <lwip/sockets.h>
#include <lwip/dhcp.h>

#include <semphr.h>
#include "form_urlencoded.h"

#include <timers_helper.h>

#include "setup.h"

#define ENDPOINT_UNKNOWN                (0)
#define ENDPOINT_INDEX                  (1)
#define ENDPOINT_SETTINGS               (2)
#define ENDPOINT_SETTINGS_UPDATE        (3)

bool got_ip = false;
esp_netif_t* setup_esp_netif = NULL;

void setup_set_esp_netif(esp_netif_t* esp_netif) {
    setup_esp_netif = esp_netif;
    //esp_netif_set_default_netif(esp_netif);
}

#define haa_sdk_wifi_set_opmode(mode)       esp_wifi_set_mode(mode)

typedef struct _wifi_network_info {
    char ssid[33];
    uint8_t bssid[6];
    char rssi[4];
    char channel[3];
    bool secure;    // 1 bit

    struct _wifi_network_info *next;
} wifi_network_info_t;

typedef struct {
    uint32_t max_body_size;
    
    TimerHandle_t auto_reboot_timer;
    
    TaskHandle_t sta_connect_timeout;
    
    TaskHandle_t ota_task;
    
    wifi_network_info_t* wifi_networks;
    SemaphoreHandle_t wifi_networks_semaph;
    
    uint8_t check_counter;
    
    bool end_setup;     // 1 bit
} wifi_config_context_t;

static wifi_config_context_t* context;

typedef struct _client {
    int fd;

    http_parser parser;
    uint8_t endpoint;
    uint8_t *body;
    uint32_t body_length;
} client_t;

void setup_mode_reset_sysparam() {
    sysparam_create_area(SYSPARAMSECTOR, SYSPARAMSIZE, true);
    sysparam_init(SYSPARAMSECTOR, 0);
}

static client_t *client_new() {
    client_t *client = calloc(1, sizeof(client_t));
    
    context->max_body_size = MAX_SETUP_BODY_LEN;
    
    do {
        client->body = malloc(context->max_body_size);
        context->max_body_size -= 200;
    } while (!client->body);

    http_parser_init(&client->parser, HTTP_REQUEST);
    client->parser.data = client;

    return client;
}

static void client_free(client_t *client) {
    if (client->body) {
        free(client->body);
    }

    free(client);
}

static void client_send(client_t *client, const char *payload, unsigned int payload_size) {
    lwip_write(client->fd, payload, payload_size);
}

static void client_send_chunk(client_t *client, const char *payload) {
    unsigned int len = strlen(payload);
    char buffer[10];
    unsigned int buffer_len = snprintf(buffer, sizeof(buffer), "%x\r\n", len);
    client_send(client, buffer, buffer_len);
    client_send(client, payload, len);
    client_send(client, "\r\n", 2);
}

static void client_send_redirect(client_t *client, int code, const char *redirect_url) {
    INFO("Redirect %s", redirect_url);
    char buffer[128];
    unsigned int len = snprintf(buffer, sizeof(buffer), "HTTP/1.1 %d \r\nLocation: %s\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", code, redirect_url);
    client_send(client, buffer, len);
}

static bool wifi_config_got_ip() {
    return got_ip;
}

static void wifi_smart_connect_task(void* arg) {
    uint8_t *best_bssid = arg;
    
    INFO("Best %02x%02x%02x%02x%02x%02x", best_bssid[0], best_bssid[1], best_bssid[2], best_bssid[3], best_bssid[4], best_bssid[5]);
    
    sysparam_set_blob(WIFI_STA_BSSID_SYSPARAM, best_bssid, 6);
    
    sdk_wifi_station_disconnect();
    
    char* wifi_ssid = NULL;
    sysparam_get_string(WIFI_STA_SSID_SYSPARAM, &wifi_ssid);
    
    char* wifi_password = NULL;
    sysparam_get_string(WIFI_STA_PASSWORD_SYSPARAM, &wifi_password);
    
    wifi_config_t sta_config = {
            .sta = {
                .scan_method = WIFI_ALL_CHANNEL_SCAN,
                .bssid_set = 1,
            },
        };
    
    strncpy((char*) sta_config.sta.ssid, wifi_ssid, sizeof(sta_config.sta.ssid) - 1);
    
    if (wifi_password) {
        strncpy((char*) sta_config.sta.password, wifi_password, sizeof(sta_config.sta.password) - 1);
    }
    
    memcpy(sta_config.sta.bssid, best_bssid, 6);

    sdk_wifi_station_set_config(&sta_config);
    
    sdk_wifi_station_connect();
    
    free(wifi_ssid);
    free(best_bssid);
    
    if (wifi_password) {
        free(wifi_password);
    }
    
    vTaskDelete(NULL);
}

static void wifi_scan_sc_done() {
    char* wifi_ssid = NULL;
    sysparam_get_string(WIFI_STA_SSID_SYSPARAM, &wifi_ssid);
    
    if (!wifi_ssid) {
        return;
    }
    
    INFO("Searching %s", wifi_ssid);
    
    int8_t best_rssi = INT8_MIN;
    uint8_t* best_bssid = malloc(6);
    unsigned int found = false;
    
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    
    wifi_ap_record_t* ap_records = (wifi_ap_record_t*) malloc(ap_count * sizeof(wifi_ap_record_t));
    esp_wifi_scan_get_ap_records(&ap_count, ap_records);

    for (unsigned int i = 0; i < ap_count; i++) {
        if (strcmp(wifi_ssid, (char*) ap_records[i].ssid) == 0) {
            INFO("RSSI %i, Ch %i - %02x%02x%02x%02x%02x%02x", ap_records[i].rssi, ap_records[i].primary, ap_records[i].bssid[0], ap_records[i].bssid[1], ap_records[i].bssid[2], ap_records[i].bssid[3], ap_records[i].bssid[4], ap_records[i].bssid[5]);

            if (ap_records[i].rssi > (best_rssi + BEST_RSSI_MARGIN)) {
                found = true;
                best_rssi = ap_records[i].rssi;
                memcpy(best_bssid, ap_records[i].bssid, 6);
            }
        }
    }
    
    if (best_rssi == INT8_MIN) {
        free(wifi_ssid);
        sdk_wifi_station_connect();
        return;
    }
    
    uint8_t *wifi_bssid = NULL;
    sysparam_get_blob(WIFI_STA_BSSID_SYSPARAM, &wifi_bssid, NULL);
    
    free(wifi_ssid);
    
    if (found) {
        if (wifi_bssid && memcmp(best_bssid, wifi_bssid, 6) == 0) {
            INFO("Same BSSID");
            free(wifi_bssid);
            if (!wifi_config_got_ip()) {
                sdk_wifi_station_connect();
            }
            return;
        }
        
        if (xTaskCreate(wifi_smart_connect_task, "WSM", (TASK_SIZE_FACTOR * 512), (void*) best_bssid, (tskIDLE_PRIORITY + 1), NULL) == pdPASS) {
            if (wifi_bssid) {
                free(wifi_bssid);
            }
            return;
        }
    }
    
    if (wifi_bssid) {
        free(wifi_bssid);
    }
    
    sdk_wifi_station_connect();
}

static void wifi_scan_sc_task(void* arg) {
    INFO("Start SC scan");
    vTaskDelay(MS_TO_TICKS(2000));
    
    if (esp_wifi_scan_start(NULL, true) == ESP_OK) {
        wifi_scan_sc_done();
    }
    
    vTaskDelete(NULL);
}

static void wifi_config_smart_connect() {
    int8_t wifi_mode = 0;
    sysparam_get_int8(WIFI_STA_MODE_SYSPARAM, &wifi_mode);
    
    if (wifi_mode < 2 || xTaskCreate(wifi_scan_sc_task, "SMA", (TASK_SIZE_FACTOR * 384), NULL, (tskIDLE_PRIORITY + 2), NULL) != pdPASS) {
        if (!wifi_config_got_ip()) {
            sdk_wifi_station_connect();
        }
    }
}

static void wifi_config_connect();

static void wifi_networks_free() {
    wifi_network_info_t* wifi_network = context->wifi_networks;
    while (wifi_network) {
        wifi_network_info_t *next = wifi_network->next;
        free(wifi_network);
        wifi_network = next;
    }
    context->wifi_networks = NULL;
}

static void wifi_scan_done_cb() {
    xSemaphoreTake(context->wifi_networks_semaph, portMAX_DELAY);

    wifi_networks_free();

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    
    wifi_ap_record_t* ap_records = (wifi_ap_record_t*) malloc(ap_count * sizeof(wifi_ap_record_t));
    esp_wifi_scan_get_ap_records(&ap_count, ap_records);

    for (unsigned int i = 0; i < ap_count; i++) {
        //INFO("%s (%i) Ch %i - %02x%02x%02x%02x%02x%02x", ap_records[i].bssid, ap_records[i].rssi, ap_records[i].primary, ap_records[i].bssid[0], ap_records[i].bssid[1], ap_records[i].bssid[2], ap_records[i].bssid[3], ap_records[i].bssid[4], ap_records[i].bssid[5]);

        wifi_network_info_t* net = context->wifi_networks;
        while (net) {
            if (!memcmp(net->bssid, ap_records[i].bssid, 6)) {
                break;
            }
            net = net->next;
        }
        
        if (!net) {
            wifi_network_info_t *net = calloc(1, sizeof(wifi_network_info_t));
            
            strncpy(net->ssid, (char*) ap_records[i].ssid, sizeof(net->ssid) - 1);
            memcpy(net->bssid, ap_records[i].bssid, 6);
            itoa(ap_records[i].rssi, net->rssi, 10);
            itoa(ap_records[i].primary, net->channel, 10);
            net->secure = ap_records[i].authmode != WIFI_AUTH_OPEN;
            net->next = context->wifi_networks;
            
            context->wifi_networks = net;
        }
    }

    xSemaphoreGive(context->wifi_networks_semaph);
}

static void wifi_scan_task(void *arg) {
    INFO("Start scan");
    
    if (!wifi_config_got_ip()) {
        sdk_wifi_station_disconnect();
        vTaskDelay(1);
    }
    
    if (esp_wifi_scan_start(NULL, true) == ESP_OK) {
        wifi_scan_done_cb();
    }
    
    if (!wifi_config_got_ip()) {
        sdk_wifi_station_connect();
    }

    vTaskDelete(NULL);
}

#define WEB_BACKGROUND_COLOR    "99ffcc"

#include "setup.html.h"

static void wifi_config_server_on_settings(client_t *client) {
    rs_esp_timer_change_period_forced(context->auto_reboot_timer, AUTO_REBOOT_LONG_TIMEOUT);
    
    xTaskCreate(wifi_scan_task, "SCA", (TASK_SIZE_FACTOR * 384), NULL, (tskIDLE_PRIORITY + 0), NULL);
    
    static const char http_prologue[] =
        "HTTP/1.1 200 \r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Cache-Control: no-store\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Connection: close\r\n"
        "\r\n";
    
    client_send(client, http_prologue, sizeof(http_prologue) - 1);
    client_send_chunk(client, html_settings_header);
    
    sysparam_status_t status;
    char* text = NULL;
    
    client_send_chunk(client, html_settings_script_start);
    
    sysparam_get_string(HAA_SCRIPT_SYSPARAM, &text);
    if (text) {
        client_send_chunk(client, text);
        free(text);
        text = NULL;
        
#if defined(HAA_SHELLY_INITIAL_SCRIPT)
    } else {
        client_send_chunk(client, HAA_SHELLY_INITIAL_SCRIPT);
#endif
        
    }
    client_send_chunk(client, html_settings_middle);
    
    client_send_chunk(client, html_settings_script_end);
    
    client_send_chunk(client, html_settings_wifi_mode_start);
    
    void send_selected() {
        client_send_chunk(client, "selected");
    }
    
    int8_t current_wifi_mode = 0;
    sysparam_get_int8(WIFI_STA_MODE_SYSPARAM, &current_wifi_mode);
    if (current_wifi_mode == 0) {
        send_selected();
    }
    client_send_chunk(client, html_wifi_mode_0);
    
    if (current_wifi_mode == 1) {
        send_selected();
    }
    client_send_chunk(client, html_wifi_mode_1);
    
    if (current_wifi_mode == 2) {
        send_selected();
    }
    client_send_chunk(client, html_wifi_mode_2);
    
    if (current_wifi_mode == 3) {
        send_selected();
    }
    client_send_chunk(client, html_wifi_mode_3);
    
    if (current_wifi_mode == 4) {
        send_selected();
    }
    client_send_chunk(client, html_wifi_mode_4);
    
    client_send_chunk(client, html_settings_flash_mode_start);
    
    uint32_t flash_id = sdk_spi_flash_get_id();
    char flash_id_text[36];
    itoa(flash_id, flash_id_text, 16);
    strcat(flash_id_text, " ");
    client_send_chunk(client, flash_id_text);
    
    uint8_t flash_mode = 0;
    
    spiflash_read(0x02 + SPIFLASHMODE_OFFSET, &flash_mode, 1);
    
    switch (flash_mode) {
        case 0:
            client_send_chunk(client, "QIO");
            break;
            
        case 1:
            client_send_chunk(client, "QOUT");
            break;
            
        case 2:
            client_send_chunk(client, "DIO");
            break;
            
        case 3:
            client_send_chunk(client, "DOUT");
            break;
            
        default:
            client_send_chunk(client, "???");
            break;
    }
    
    client_send_chunk(client, html_settings_flash_mode);
    
    sysparam_get_string(WIFI_STA_SSID_SYSPARAM, &text);
    if (text) {
        client_send_chunk(client, text);
        free(text);
        text = NULL;
        
        uint8_t* wifi_bssid = NULL;
        sysparam_get_blob(WIFI_STA_BSSID_SYSPARAM, &wifi_bssid, NULL);
        if (wifi_bssid) {
            text = malloc(16);
            snprintf(text, 16, " (%02x%02x%02x%02x%02x%02x)", wifi_bssid[0], wifi_bssid[1], wifi_bssid[2], wifi_bssid[3], wifi_bssid[4], wifi_bssid[5]);
            free(wifi_bssid);
            
            client_send_chunk(client, text);
            free(text);
            text = NULL;
        }
        
    } else {
        client_send_chunk(client, "NONE");
    }
    client_send_chunk(client, html_settings_current_wifi);
    
    // Wifi Networks
    char buffer[150];
    char bssid[13];
    if (xSemaphoreTake(context->wifi_networks_semaph, MS_TO_TICKS(4000))) {
        wifi_network_info_t* net = context->wifi_networks;
        while (net) {
            snprintf(bssid, sizeof(bssid), "%02x%02x%02x%02x%02x%02x", net->bssid[0], net->bssid[1], net->bssid[2], net->bssid[3], net->bssid[4], net->bssid[5]);
            snprintf(
                buffer, sizeof(buffer),
                html_network_item,
                net->secure ? "sec" : "unsec", bssid, net->ssid, net->ssid, net->rssi, net->channel, bssid
            );
            client_send_chunk(client, buffer);
            
            net = net->next;
        }

        xSemaphoreGive(context->wifi_networks_semaph);
    }
    
    client_send_chunk(client, html_settings_wifi);
    
    // Custom repo server
    sysparam_get_string(CUSTOM_REPO_SYSPARAM, &text);
    if (text) {
        client_send_chunk(client, text);
        free(text);
    }
    client_send_chunk(client, html_settings_reposerver);
    
    int32_t int32_value;
    
    status = sysparam_get_int32(PORT_NUMBER_SYSPARAM, &int32_value);
    if (status == SYSPARAM_OK) {
        char str_port[8];
        itoa(int32_value, str_port, 10);
        client_send_chunk(client, str_port);
    } else {
        client_send_chunk(client, "80");
    }
    client_send_chunk(client, html_settings_repoport);
    
    int8_t int8_value = 0;
    sysparam_get_int8(PORT_SECURE_SYSPARAM, &int8_value);
    if (int8_value == 1) {
        client_send_chunk(client, "checked");
    }
    client_send_chunk(client, html_settings_repossl);

    client_send_chunk(client, "");
}

static void wifi_config_context_free(wifi_config_context_t *context) {
    wifi_networks_free();

    free(context);
    context = NULL;
}
    
static void wifi_config_remove_ap_settings() {
    sysparam_erase(WIFI_AP_ENABLE_SYSPARAM);
    sysparam_erase(WIFI_AP_PASSWORD_SYSPARAM);
}

static void wifi_config_server_on_settings_update_task(void* args) {
    client_t* client = args;
    context->end_setup = true;
    
    while (context->end_setup) {
        vTaskDelay(MS_TO_TICKS(1000));
    }
    
    sdk_wifi_station_disconnect();
    
    INFO("Update settings");
    
    wifi_config_context_free(context);
    
    form_param_t *form = form_params_parse((char *) client->body);
    free(client->body);
    
    if (!form) {
        ERROR("DRAM");
        
    } else {
        form_param_t *reset_sys_param = form_params_find(form, "rsy");
        
        if (reset_sys_param) {
            int32_t last_config_number = 0;
            sysparam_get_int32(LAST_CONFIG_NUMBER_SYSPARAM, &last_config_number);
            
            char* installer_version_string = NULL;
            sysparam_get_string(INSTALLER_VERSION_SYSPARAM, &installer_version_string);
            
            char* haamain_version_string = NULL;
            sysparam_get_string(HAAMAIN_VERSION_SYSPARAM, &haamain_version_string);
            
            int8_t saved_pairing_count = 0;
            sysparam_get_int8(HOMEKIT_PAIRING_COUNT_SYSPARAM, &saved_pairing_count);
            
            setup_mode_reset_sysparam();
            
            if (last_config_number > 0) {
                sysparam_set_int32(LAST_CONFIG_NUMBER_SYSPARAM, last_config_number);
            }
            
            if (installer_version_string) {
                sysparam_set_string(INSTALLER_VERSION_SYSPARAM, installer_version_string);
            }
            
            if (haamain_version_string) {
                sysparam_set_string(HAAMAIN_VERSION_SYSPARAM, haamain_version_string);
            }
            
            if (saved_pairing_count > 0) {
                sysparam_set_int8(HOMEKIT_PAIRING_COUNT_SYSPARAM, saved_pairing_count);
            }
            
        } else {
            form_param_t *nowifi_param = form_params_find(form, "now");
            form_param_t *fm_param = form_params_find(form, "fm");
            form_param_t *ssid_param = form_params_find(form, "sid");
            form_param_t *bssid_param = form_params_find(form, "bid");
            form_param_t *password_param = form_params_find(form, "psw");
            form_param_t *reposerver_param = form_params_find(form, "ser");
            form_param_t *repoport_param = form_params_find(form, "prt");
            form_param_t *repossl_param = form_params_find(form, "ssl");
            
            form_param_t *conf_param = form_params_find(form, "cnf");
            form_param_t *wifimode_param = form_params_find(form, "wm");
            
            // Remove saved states
            int32_t hk_total_serv = 0;
            sysparam_get_int32(TOTAL_SERV_SYSPARAM, &hk_total_serv);
            
            char saved_state_id[8];
            for (int serv = 1; serv <= hk_total_serv; serv++) {
                for (int ch = 0; ch < HIGH_HOMEKIT_CH_NUMBER; ch++) {
                    uint32_t int_saved_state_id = (serv * 100) + ch;
                    itoa(int_saved_state_id, saved_state_id, 10);
                    sysparam_erase(saved_state_id);
                }
            }
            
            if (conf_param && conf_param->value) {
                sysparam_set_string(HAA_SCRIPT_SYSPARAM, conf_param->value);
            } else {
                sysparam_erase(HAA_SCRIPT_SYSPARAM);
            }
            
            sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 0);
            
            if (nowifi_param) {
                sysparam_erase(WIFI_STA_SSID_SYSPARAM);
                sysparam_erase(WIFI_STA_PASSWORD_SYSPARAM);
                sysparam_erase(WIFI_STA_BSSID_SYSPARAM);
                sysparam_erase(WIFI_STA_MODE_SYSPARAM);
                wifi_config_remove_ap_settings();
            }
            
            if (reposerver_param && reposerver_param->value) {
                sysparam_set_string(CUSTOM_REPO_SYSPARAM, reposerver_param->value);
            } else {
                sysparam_erase(CUSTOM_REPO_SYSPARAM);
            }
            
            if (repoport_param && repoport_param->value) {
                const int32_t port = strtol(repoport_param->value, NULL, 10);
                sysparam_set_int32(PORT_NUMBER_SYSPARAM, port);
            } else {
                sysparam_set_int32(PORT_NUMBER_SYSPARAM, 80);
            }
            
            if (repossl_param) {
                sysparam_set_int8(PORT_SECURE_SYSPARAM, 1);
            } else {
                sysparam_set_int8(PORT_SECURE_SYSPARAM, 0);
            }
            
            if (ssid_param && ssid_param->value) {
                sysparam_set_string(WIFI_STA_SSID_SYSPARAM, ssid_param->value);
                
                if (bssid_param && bssid_param->value && strlen(bssid_param->value) == 12) {
                    uint8_t bssid[6];
                    char hex[3];
                    hex[2] = 0;
                    
                    for (int i = 0; i < 6; i++) {
                        hex[0] = bssid_param->value[(i * 2)];
                        hex[1] = bssid_param->value[(i * 2) + 1];
                        bssid[i] = (uint8_t) strtol(hex, NULL, 16);
                    }
                    
                    sysparam_set_blob(WIFI_STA_BSSID_SYSPARAM, bssid, 6);
                    
                } else {
                    sysparam_erase(WIFI_STA_BSSID_SYSPARAM);
                }
                
                if (password_param && password_param->value && strlen(password_param->value) >= 8) {
                    sysparam_set_string(WIFI_STA_PASSWORD_SYSPARAM, password_param->value);
                } else {
                    sysparam_erase(WIFI_STA_PASSWORD_SYSPARAM);
                }
                
                wifi_config_remove_ap_settings();
            }
            
            if (fm_param && fm_param->value) {
                int8_t new_fm = strtol(fm_param->value, NULL, 10);
                if (new_fm >= 0 && new_fm <= 3) {
                    uint8_t* sector = malloc(SPI_FLASH_SECTOR_SIZE);
                    if (sector) {
                        spiflash_read(0x0 + SPIFLASHMODE_OFFSET, sector, SPI_FLASH_SECTOR_SIZE);
                        
                        if (sector[2] != new_fm) {
                            sector[2] = new_fm;
                            spiflash_erase_sector(0x0 + SPIFLASHMODE_OFFSET);
                            spiflash_write(0x0 + SPIFLASHMODE_OFFSET, sector, SPI_FLASH_SECTOR_SIZE);
                        }
                        
                        free(sector);
                    }
                }
            }
            
            if (wifimode_param && wifimode_param->value) {
                int8_t current_wifi_mode = 0;
                sysparam_get_int8(WIFI_STA_MODE_SYSPARAM, &current_wifi_mode);
                
                int8_t new_wifi_mode = strtol(wifimode_param->value, NULL, 10);
                
                if (current_wifi_mode != new_wifi_mode) {
                    sysparam_set_int8(WIFI_STA_MODE_SYSPARAM, new_wifi_mode);
                    wifi_config_remove_ap_settings();
                }
            }
        }
    }
    
    INFO("Reboot");
    vTaskDelay(MS_TO_TICKS(1000));
    
    sdk_system_restart();
}

static int wifi_config_server_on_url(http_parser *parser, const char *data, unsigned int length) {
    client_t *client = (client_t*) parser->data;

    client->endpoint = ENDPOINT_UNKNOWN;
    if (parser->method == HTTP_GET) {
        if (!strncmp(data, "/sn", length)) {
            client->endpoint = ENDPOINT_SETTINGS;
        } else if (!strncmp(data, "/", length)) {
            client->endpoint = ENDPOINT_INDEX;
        }
    } else if (parser->method == HTTP_POST) {
        if (!strncmp(data, "/sn", length)) {
            client->endpoint = ENDPOINT_SETTINGS_UPDATE;
        }
    }

    if (client->endpoint == ENDPOINT_UNKNOWN) {
        char *url = strndup(data, length);
        ERROR("Unknown %s %s", http_method_str(parser->method), url);
        free(url);
    }

    return 0;
}

static int wifi_config_server_on_body(http_parser *parser, const char *data, unsigned int length) {
    client_t *client = parser->data;
    
    //client->body = realloc(client->body, client->body_length + length + 1);
    if ( (client->body_length + length) < context->max_body_size ) {
        memcpy(client->body + client->body_length, data, length);
        client->body_length += length;
        client->body[client->body_length] = 0;
    } else {
        context->max_body_size = client->body_length;
    }

    return 0;
}

static int wifi_config_server_on_message_complete(http_parser *parser) {
    client_t *client = parser->data;

    switch(client->endpoint) {
        case ENDPOINT_INDEX:
        case ENDPOINT_UNKNOWN:
            client_send_redirect(client, 301, "/sn");
            break;
            
        case ENDPOINT_SETTINGS:
            wifi_config_server_on_settings(client);
            break;
            
        case ENDPOINT_SETTINGS_UPDATE:
            rs_esp_timer_change_period_forced(context->auto_reboot_timer, AUTO_REBOOT_LONG_TIMEOUT);
            if (context->sta_connect_timeout) {
                vTaskDelete(context->sta_connect_timeout);
            }
            
            xTaskCreate(wifi_config_server_on_settings_update_task, "UDP", (TASK_SIZE_FACTOR * 512), client, (tskIDLE_PRIORITY + 1), NULL);
            return 0;
        
        /*
        case ENDPOINT_UNKNOWN:
            INFO("Unknown");
            client_send_redirect(client, 302, "http://192.168.4.1:4567/sn");
            break;
        */
    }

    if (client->body) {
        free(client->body);
        client->body = NULL;
        client->body_length = 0;
    }

    return 0;
}

static http_parser_settings wifi_config_http_parser_settings = {
    .on_url = wifi_config_server_on_url,
    .on_body = wifi_config_server_on_body,
    .on_message_complete = wifi_config_server_on_message_complete,
};

static void http_task(void *arg) {
    INFO("Start WEB");

    context->end_setup = false;
    
    struct sockaddr_in serv_addr;
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(WIFI_CONFIG_SERVER_PORT);
    
    bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    listen(listenfd, 2);

    char data[128];
    
    for (;;) {
        int fd = accept(listenfd, (struct sockaddr *)NULL, (socklen_t *)NULL);
        if (fd < 0) {
            vTaskDelay(MS_TO_TICKS(200));
            continue;
        }

        const struct timeval rcvtimeout = { 1, 0 };
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &rcvtimeout, sizeof(rcvtimeout));
        
        const int yes = 1;
        setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes));

        const int interval = 5;
        setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));

        const int maxpkt = 4;
        setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &maxpkt, sizeof(maxpkt));

        client_t *client = client_new();
        client->fd = fd;

        //int data_total = 0;
        
        for (;;) {
            int data_len = lwip_read(client->fd, data, sizeof(data));
            
            //data_total += data_len;
            //INFO("lwip_read %d, %d", data_len, data_total);

            if (data_len > 0) {
                http_parser_execute(
                    &client->parser, &wifi_config_http_parser_settings,
                    data, data_len
                );
            } else {
                break;
            }
        }
        
        if (context->end_setup) {
            static const char payload[] = "HTTP/1.1 200\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<center>OK</center>";
            client_send(client, payload, sizeof(payload) - 1);
            vTaskDelay(MS_TO_TICKS(300));
            lwip_close(client->fd);
            break;
        }
        
        //INFO("Conn closed");
        
        lwip_close(client->fd);
        client_free(client);
    }
    
    context->end_setup = false;
    
    INFO("Stop WEB");
    vTaskDelete(NULL);
}

static void wifi_config_softap_start() {
    haa_sdk_wifi_set_opmode(STATIONAP_MODE);
    //sdk_wifi_set_sleep_type(WIFI_SLEEP_NONE);
    
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);
    
    const uint8_t wifi_channel = ((hwrand() % 3) * 5) + 1;
    
    wifi_config_t softap_config = {
        .ap = {
            .channel = wifi_channel,
            .authmode = WIFI_AUTH_OPEN,
            .max_connection = 2,
            .ssid_hidden = 0,
        },
    };
    
    softap_config.ap.ssid_len = snprintf(
        (char*) softap_config.ap.ssid, sizeof(softap_config.ap.ssid),
        "HAA-%02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]
    );
    
    INFO("Wifi AP %s Ch%i", softap_config.ap.ssid, softap_config.ap.channel);
    
    esp_netif_t* ap_netif = esp_netif_create_default_wifi_ap();
    assert(ap_netif);
    esp_netif_ip_info_t ap_ip;
    IP4_ADDR(&ap_ip.ip, 192, 168, 4, 1);
    IP4_ADDR(&ap_ip.netmask, 255, 255, 255, 0);
    IP4_ADDR(&ap_ip.gw, 0, 0, 0, 0);
    esp_netif_set_ip_info(ap_netif, &ap_ip);
    
    esp_wifi_set_config(WIFI_IF_AP, &softap_config);
    esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);

    
    context->wifi_networks_semaph = xSemaphoreCreateBinary();
    xSemaphoreGive(context->wifi_networks_semaph);
    
    xTaskCreate(http_task, "WEB", (TASK_SIZE_FACTOR * 640), NULL, (tskIDLE_PRIORITY + 1), NULL);
}

static void auto_reboot_run() {
    INFO("Auto Reboot");

    vTaskDelay(MS_TO_TICKS(500));
    
    sdk_system_restart();
}

static void wifi_config_sta_connect_timeout_task() {
    for (;;) {
        vTaskDelay(MS_TO_TICKS(1000));

        if (wifi_config_got_ip()) {
            vTaskResume(context->ota_task);
            
            wifi_config_context_free(context);
            
            rs_esp_timer_start_forced(rs_esp_timer_create(AUTO_REBOOT_ON_HANG_OTA_TIMEOUT, pdFALSE, NULL, auto_reboot_run));
            
            break;

        } else if (sdk_wifi_get_opmode() == STATION_MODE) {
            context->check_counter++;
            if (context->check_counter > 240) {
                auto_reboot_run();
            }
        }
    }
    
    vTaskDelete(NULL);
}

static void on_got_ip(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    got_ip = true;
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    INFO("IP:" IPSTR "/" IPSTR " GW:" IPSTR, IP2STR(&event->ip_info.ip), IP2STR(&event->ip_info.netmask), IP2STR(&event->ip_info.gw));
    
    char* buf = malloc(16);
    snprintf(buf, 16, IPSTR, IP2STR(&event->ip_info.ip));
    adv_logger_set_ip_address(buf);
}
        
static void on_disconnect(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    ERROR("Wifi disconnected");
    got_ip = false;
    sdk_wifi_station_connect();
}

static void wifi_config_connect() {
    sysparam_set_string(INSTALLER_VERSION_SYSPARAM, INSTALLER_VERSION);
    
    char *wifi_ssid = NULL;
    sysparam_get_string(WIFI_STA_SSID_SYSPARAM, &wifi_ssid);
    
    wifi_config_t sta_config = {
        .sta = {
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            .bssid_set = 0,
        },
    };
    
    sdk_wifi_station_set_config(&sta_config);
    esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
    esp_wifi_set_ps(WIFI_PS_NONE);
    
    sdk_wifi_station_disconnect();
    vTaskDelay(1);

    char* wifi_password = NULL;
    sysparam_get_string(WIFI_STA_PASSWORD_SYSPARAM, &wifi_password);
    
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL);
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_disconnect, NULL);
    
    strncpy((char*) sta_config.sta.ssid, wifi_ssid, sizeof(sta_config.sta.ssid) - 1);
    free(wifi_ssid);
    
    if (wifi_password) {
        strncpy((char*) sta_config.sta.password, wifi_password, sizeof(sta_config.sta.password) - 1);
        free(wifi_password);
    }
    
    int8_t wifi_mode = 0;
    sysparam_get_int8(WIFI_STA_MODE_SYSPARAM, &wifi_mode);
    
    uint8_t* wifi_bssid = NULL;
    sysparam_get_blob(WIFI_STA_BSSID_SYSPARAM, &wifi_bssid, NULL);
    
    INFO_NNL("BSSID ");
    if (wifi_bssid) {
        INFO("%02x%02x%02x%02x%02x%02x", wifi_bssid[0], wifi_bssid[1], wifi_bssid[2], wifi_bssid[3], wifi_bssid[4], wifi_bssid[5]);
    } else {
        INFO("-");
    }
    
    INFO_NNL("Mode ");
    if (wifi_mode < 2) {
        if (wifi_mode == 1 && wifi_bssid) {
            sta_config.sta.bssid_set = 1;
            memcpy(sta_config.sta.bssid, wifi_bssid, 6);
            INFO("Forced");
            
            //INFO("Wifi Mode: Forced BSSID %02x%02x%02x%02x%02x%02x", sta_config.bssid[0], sta_config.bssid[1], sta_config.bssid[2], sta_config.bssid[3], sta_config.bssid[4], sta_config.bssid[5]);

        } else {
            INFO("Normal");
        }
        
        haa_sdk_wifi_set_opmode(STATION_MODE);
        sdk_wifi_station_set_config(&sta_config);
        sdk_wifi_station_set_auto_connect(true);
        
        sdk_wifi_station_connect();
        
    } else {
        INFO("Roaming");
        sysparam_erase(WIFI_STA_BSSID_SYSPARAM);
        haa_sdk_wifi_set_opmode(STATION_MODE);
        sdk_wifi_station_set_config(&sta_config);
        sdk_wifi_station_set_auto_connect(true);
        
        if (wifi_mode == 4) {
            xTaskCreate(wifi_scan_sc_task, "SMA", (TASK_SIZE_FACTOR * 384), NULL, (tskIDLE_PRIORITY + 2), NULL);
        } else {
            wifi_config_smart_connect();
        }
        
        vTaskDelay(MS_TO_TICKS(5000));
    }
    
    if (wifi_bssid) {
        free(wifi_bssid);
    }
}

static void wifi_config_station_connect() {
    vTaskDelay(1);
    
    int8_t setup_mode = 3;
    
    char *wifi_ssid = NULL;
    sysparam_get_string(WIFI_STA_SSID_SYSPARAM, &wifi_ssid);
    if (wifi_ssid) {
        free(wifi_ssid);
        
        wifi_config_connect();
        
        sysparam_get_int8(HAA_SETUP_MODE_SYSPARAM, &setup_mode);
    }
    
    if (setup_mode == 0) {
        INFO("* NORMAL");
        
        sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 1);
        
        xTaskCreate(wifi_config_sta_connect_timeout_task, "STI", (TASK_SIZE_FACTOR * 640), NULL, (tskIDLE_PRIORITY + 1), &context->sta_connect_timeout);
        
    } else {
        INFO("* SETUP");
        
        wifi_config_softap_start();
        
        adv_logger_close_buffered_task();
        vTaskDelete(context->ota_task);
        sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 0);
        
        if (setup_mode == 1) {
            context->auto_reboot_timer = rs_esp_timer_create(AUTO_REBOOT_TIMEOUT, pdFALSE, NULL, auto_reboot_run);
            rs_esp_timer_start_forced(context->auto_reboot_timer);
        }
    }
    
    esp_wifi_start();
    
    vTaskDelete(NULL);
}

void wifi_config_init(TaskHandle_t xHandle) {
    INFO("Wifi init");
    
    context = calloc(1, sizeof(wifi_config_context_t));
    
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);
    char* setup_hostname = malloc(22);
    snprintf(setup_hostname, 22, "HAA-%02X%02X%02X-InstallerB", macaddr[3], macaddr[4], macaddr[5]);
    esp_netif_set_hostname(setup_esp_netif, setup_hostname);
    free(setup_hostname);
    
    context->ota_task = xHandle;
    
    xTaskCreate(wifi_config_station_connect, "WCO", (TASK_SIZE_FACTOR * 512), NULL, (tskIDLE_PRIORITY + 1), NULL);
}
