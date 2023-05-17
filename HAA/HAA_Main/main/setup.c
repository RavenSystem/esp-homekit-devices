/*
 * Home Accessory Architect
 *
 * Copyright 2019-2023 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

/*
 * Based on esp-wifi-config library by Maxim Kulkin (@MaximKulkin), licensed under the MIT License.
 * https://github.com/maximkulkin/esp-wifi-config
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#ifdef ESP_PLATFORM

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_netif.h" 
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

#else   // ESP_OPEN_RTOS

#include <sysparam.h>
#include <FreeRTOS.h>
#include <espressif/esp_common.h>
#include <esplibs/libmain.h>
#include <spiflash.h>
#include <http-parser/http_parser.h>
#include <dhcpserver.h>
#include <rboot-api.h>

#endif


#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>
#include <lwip/dns.h>
#include <lwip/dhcp.h>

#include <semphr.h>
#include "form_urlencoded.h"

#include <homekit/homekit.h>
#include <timers_helper.h>

#include "header.h"

#define SETUP_ANNOUNCER_DESTINATION     "255.255.255.255"
#define SETUP_ANNOUNCER_PORT            "4567"

#define ENDPOINT_UNKNOWN            (0)
#define ENDPOINT_INDEX              (1)
#define ENDPOINT_SETTINGS           (2)
#define ENDPOINT_SETTINGS_UPDATE    (3)

#ifdef ESP_PLATFORM
esp_ip4_addr_t ip_addr;
esp_ip4_addr_t gw_addr;
esp_netif_t* setup_esp_netif = NULL;

void setup_set_esp_netif(esp_netif_t* esp_netif) {
    setup_esp_netif = esp_netif;
    //esp_netif_set_default_netif(esp_netif);
}

esp_netif_t* setup_get_esp_netif() {
    return setup_esp_netif;
}

void setup_set_boot_installer() {
    esp_partition_t* partition_boot1 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
    
    if (!partition_boot1) {
        partition_boot1 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
    }
    
    esp_ota_set_boot_partition(partition_boot1);
}
#endif

typedef struct _wifi_network_info {
    char ssid[33];
    uint8_t bssid[6];
    char rssi[4];
    char channel[3];
    bool secure;

    struct _wifi_network_info *next;
} wifi_network_info_t;

typedef struct {
    char* ssid_prefix;
    char* custom_hostname;
    int32_t param;
    size_t max_body_size;
    void (*on_wifi_ready)();
    
    struct addrinfo* res;

    TimerHandle_t auto_reboot_timer;
    
    TaskHandle_t sta_connect_timeout;
    TaskHandle_t setup_announcer;
    
    wifi_network_info_t* wifi_networks;
    SemaphoreHandle_t wifi_networks_semaph;

    uint8_t check_counter;

#ifdef ESP_PLATFORM
    uint8_t wifi_sleep_mode;
    bool bandwidth_40;  // 1 bit
#endif
    
    bool end_setup; // 1 bit
} wifi_config_context_t;

static wifi_config_context_t* context;

typedef struct _client {
    int32_t fd;

    http_parser parser;
    uint8_t endpoint;
    uint8_t* body;
    size_t body_length;
} client_t;

static void wifi_config_station_connect();

inline static void setup_mode_reset_sysparam() {
    sysparam_create_area(SYSPARAMSECTOR, SYSPARAMSIZE, true);
    sysparam_init(SYSPARAMSECTOR, 0);
}

static client_t *client_new() {
    client_t *client = malloc(sizeof(client_t));
    memset(client, 0, sizeof(client_t));
    
    context->max_body_size = MAX_SETUP_BODY_LEN;
    
    do {
        client->body = malloc(context->max_body_size);
        context->max_body_size -= 200;
    } while (!client->body);
    
    http_parser_init(&client->parser, HTTP_REQUEST);
    client->parser.data = client;

    return client;
}

static void client_free(client_t* client) {
    if (!client) {
        return;
    }
    
    if (client->body) {
        free(client->body);
    }

    free(client);
    client = NULL;
}

static void client_send(client_t *client, const char *payload, size_t payload_size) {
    lwip_write(client->fd, payload, payload_size);
}

static void client_send_chunk(client_t *client, const char *payload) {
    size_t len = strlen(payload);
    char buffer[10];
    size_t buffer_len = snprintf(buffer, sizeof(buffer), "%x\r\n", len);
    client_send(client, buffer, buffer_len);
    client_send(client, payload, len);
    client_send(client, "\r\n", 2);
}

static void client_send_redirect(client_t *client, int code, const char *redirect_url) {
    INFO("Redirect %s", redirect_url);
    char buffer[128];
    size_t len = snprintf(buffer, sizeof(buffer), "HTTP/1.1 %d \r\nLocation: %s\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", code, redirect_url);
    client_send(client, buffer, len);
}

int wifi_config_get_ip() {
#ifdef ESP_PLATFORM
    if (ip_addr.addr != 0) {
        return ip4_addr4_16(&ip_addr);
    }
#else
    struct ip_info info;
    if (sdk_wifi_get_ip_info(STATION_IF, &info) && info.ip.addr != 0) {
        return ip4_addr4_16(&info.ip);
    }
#endif
    
    return -1;
}

uint32_t wifi_config_get_full_gw() {
#ifdef ESP_PLATFORM
    return gw_addr.addr;
#else
    struct ip_info info;
    if (sdk_wifi_get_ip_info(STATION_IF, &info)) {
        return info.gw.addr;
    }
    
    return 0;
#endif
}

#ifndef ESP_PLATFORM
static void wifi_config_toggle_phy_mode(const uint8_t phy) {
    switch (phy) {
        /* Not used
        case 1:
            sdk_wifi_set_phy_mode(PHY_MODE_11B);
            break;
        */
        case 2:
            sdk_wifi_set_phy_mode(PHY_MODE_11G);
            break;
            
        case 3:
            sdk_wifi_set_phy_mode(PHY_MODE_11N);
            break;
            
        case 4:
            if (sdk_wifi_get_phy_mode() == PHY_MODE_11N) {
                sdk_wifi_set_phy_mode(PHY_MODE_11G);
            } else {
                sdk_wifi_set_phy_mode(PHY_MODE_11N);
            }
            break;
            
        default:
            break;
    }
}
#endif

static void wifi_smart_connect_task(void* arg) {
    uint8_t *best_bssid = arg;
    
    INFO("Best %02x%02x%02x%02x%02x%02x", best_bssid[0], best_bssid[1], best_bssid[2], best_bssid[3], best_bssid[4], best_bssid[5]);
    
    sysparam_set_blob(WIFI_BSSID_SYSPARAM, best_bssid, (size_t) 6);
    
    sdk_wifi_station_disconnect();
    
    char* wifi_ssid = NULL;
    sysparam_get_string(WIFI_SSID_SYSPARAM, &wifi_ssid);

    char *wifi_password = NULL;
    sysparam_get_string(WIFI_PASSWORD_SYSPARAM, &wifi_password);
    
#ifdef ESP_PLATFORM
    wifi_config_t sta_config = {
            .sta = {
                .password = "",
                .scan_method = WIFI_ALL_CHANNEL_SCAN,
                .rm_enabled = 1,
                .btm_enabled = 1 ,
                .mbo_enabled = 1,
                .pmf_cfg.capable = 1,
                .ft_enabled = 1,
                .bssid_set = 1,
            },
        };
    
    strncpy((char *)sta_config.sta.ssid, wifi_ssid, sizeof(sta_config.sta.ssid) - 1);
    
    if (wifi_password) {
        strncpy((char *)sta_config.sta.password, wifi_password, sizeof(sta_config.sta.password) - 1);
    }
    
    memcpy(sta_config.sta.bssid, best_bssid, 6);
#else
    struct sdk_station_config sta_config;
    memset(&sta_config, 0, sizeof(sta_config));
    
    strncpy((char *)sta_config.ssid, wifi_ssid, sizeof(sta_config.ssid) - 1);
    
    if (wifi_password) {
       strncpy((char *)sta_config.password, wifi_password, sizeof(sta_config.password) - 1);
    }
    
    sta_config.bssid_set = 1;
    memcpy(sta_config.bssid, best_bssid, 6);
#endif
    
    sdk_wifi_station_set_config(&sta_config);
    
#ifndef ESP_PLATFORM
    sdk_wifi_station_set_auto_connect(true);
    int8_t phy_mode = 3;
    sysparam_get_int8(WIFI_LAST_WORKING_PHY_SYSPARAM, &phy_mode);
    wifi_config_toggle_phy_mode(phy_mode);
#endif
    
    sdk_wifi_station_connect();
    
    free(wifi_ssid);
    free(best_bssid);
    
    if (wifi_password) {
        free(wifi_password);
    }
    
    vTaskDelete(NULL);
}

#ifdef ESP_PLATFORM
static void wifi_scan_sc_done() {
    char* wifi_ssid = NULL;
    sysparam_get_string(WIFI_SSID_SYSPARAM, &wifi_ssid);
    
    if (!wifi_ssid) {
        return;
    }
    
    INFO("Search BSSID for %s", wifi_ssid);
    
    int8_t best_rssi = INT8_MIN;
    uint8_t* best_bssid = malloc(6);
    int found = false;
    
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    
    wifi_ap_record_t* ap_records = (wifi_ap_record_t*) malloc(ap_count * sizeof(wifi_ap_record_t));
    esp_wifi_scan_get_ap_records(&ap_count, ap_records);
    
    for (int i = 0; i < ap_count; i++) {
        if (strcmp(wifi_ssid, (char*) ap_records[i].ssid) == 0) {
            INFO("RSSI %i, Ch %i - %02x%02x%02x%02x%02x%02x", ap_records[i].rssi, ap_records[i].primary, ap_records[i].bssid[0], ap_records[i].bssid[1], ap_records[i].bssid[2], ap_records[i].bssid[3], ap_records[i].bssid[4], ap_records[i].bssid[5]);
            
            if (ap_records[i].rssi > (best_rssi + BEST_RSSI_MARGIN)) {
                found = true;
                best_rssi = ap_records[i].rssi;
                memcpy(best_bssid, ap_records[i].bssid, 6);
            }
        }
    }
    
#else
    
static void wifi_scan_sc_done(void* arg, sdk_scan_status_t status) {
    if (status != SCAN_OK) {
        ERROR("SC scan");
        if (wifi_config_get_ip() < 0) {
            sdk_wifi_station_connect();
        }
    }

    char* wifi_ssid = NULL;
    sysparam_get_string(WIFI_SSID_SYSPARAM, &wifi_ssid);
    
    if (!wifi_ssid) {
        return;
    }
    
    INFO("Search BSSID for %s", wifi_ssid);
    
    struct sdk_bss_info* bss = (struct sdk_bss_info*) arg;
    // first one is invalid
    bss = bss->next.stqe_next;

    int8_t best_rssi = INT8_MIN;
    uint8_t* best_bssid = malloc(6);
    int found = false;
    while (bss) {
        if (strcmp(wifi_ssid, (char*) bss->ssid) == 0) {
            INFO("RSSI %i, Ch %i - %02x%02x%02x%02x%02x%02x", bss->rssi, bss->channel, bss->bssid[0], bss->bssid[1], bss->bssid[2], bss->bssid[3], bss->bssid[4], bss->bssid[5]);

            if (bss->rssi > (best_rssi + BEST_RSSI_MARGIN)) {
                found = true;
                best_rssi = bss->rssi;
                memcpy(best_bssid, bss->bssid, 6);
            }
        }
        
        bss = bss->next.stqe_next;
    }
#endif
    
    if (best_rssi == INT8_MIN) {
        free(wifi_ssid);
        sdk_wifi_station_connect();
        return;
    }
    
    uint8_t *wifi_bssid = NULL;
    size_t len = 6;
    sysparam_get_blob(WIFI_BSSID_SYSPARAM, &wifi_bssid, &len);
    
    free(wifi_ssid);
    
    if (found) {
        if (wifi_bssid && memcmp(best_bssid, wifi_bssid, 6) == 0) {
            INFO("Same BSSID");
            free(wifi_bssid);
            if (wifi_config_get_ip() < 0) {
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
    
#ifdef ESP_PLATFORM
    if (esp_wifi_scan_start(NULL, true) == ESP_OK) {
        wifi_scan_sc_done();
    }
#else
    sdk_wifi_station_scan(NULL, wifi_scan_sc_done);
#endif
 
    vTaskDelete(NULL);
}

void wifi_config_smart_connect() {
    int8_t wifi_mode = 0;
    sysparam_get_int8(WIFI_MODE_SYSPARAM, &wifi_mode);
    
    if (wifi_mode < 2 || xTaskCreate(wifi_scan_sc_task, "SMA", (TASK_SIZE_FACTOR * 384), NULL, (tskIDLE_PRIORITY + 2), NULL) != pdPASS) {
        if (wifi_config_get_ip() < 0) {
            sdk_wifi_station_connect();
        }
    }
}

#ifdef ESP_PLATFORM
uint8_t wifi_config_connect(const uint8_t mode, const bool with_reset);
#else
uint8_t wifi_config_connect(const uint8_t mode, const uint8_t phy, const bool with_reset);
#endif
    
void wifi_config_reset() {
    INFO("Wifi clean");
    sdk_wifi_station_disconnect();
    
#ifndef ESP_PLATFORM
    struct sdk_station_config sta_config;
    memset(&sta_config, 0, sizeof(sta_config));

    strncpy((char*) sta_config.ssid, "none", sizeof(sta_config.ssid) - 1);
    sta_config.ssid[sizeof(sta_config.ssid) - 1] = 0;
    
    sdk_wifi_station_set_config(&sta_config);
    sdk_wifi_station_set_auto_connect(false);
#endif
    
    sdk_wifi_station_connect();
}

static void wifi_networks_free() {
    wifi_network_info_t* wifi_network = context->wifi_networks;
    while (wifi_network) {
        wifi_network_info_t* next = wifi_network->next;
        free(wifi_network);
        wifi_network = next;
    }
    context->wifi_networks = NULL;
}

#ifdef ESP_PLATFORM
static void wifi_scan_done_cb() {
    xSemaphoreTake(context->wifi_networks_semaph, portMAX_DELAY);

    wifi_networks_free();

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    
    wifi_ap_record_t* ap_records = (wifi_ap_record_t*) malloc(ap_count * sizeof(wifi_ap_record_t));
    esp_wifi_scan_get_ap_records(&ap_count, ap_records);

    for (int i = 0; i < ap_count; i++) {
        //INFO("%s (%i) Ch %i - %02x%02x%02x%02x%02x%02x", ap_records[i].bssid, ap_records[i].rssi, ap_records[i].primary, ap_records[i].bssid[0], ap_records[i].bssid[1], ap_records[i].bssid[2], ap_records[i].bssid[3], ap_records[i].bssid[4], ap_records[i].bssid[5]);

        wifi_network_info_t* net = context->wifi_networks;
        while (net) {
            if (!memcmp(net->bssid, ap_records[i].bssid, 6)) {
                break;
            }
            net = net->next;
        }
        
        if (!net) {
            wifi_network_info_t *net = malloc(sizeof(wifi_network_info_t));
            memset(net, 0, sizeof(*net));
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
#else
static void wifi_scan_done_cb(void *arg, sdk_scan_status_t status) {
    if (status != SCAN_OK || !context) {
        ERROR("Wifi scan");
        return;
    }
    
    xSemaphoreTake(context->wifi_networks_semaph, portMAX_DELAY);
    
    wifi_networks_free();
    
    struct sdk_bss_info *bss = (struct sdk_bss_info *)arg;
    // first one is invalid
    bss = bss->next.stqe_next;
    
    while (bss) {
        //INFO("%s (%i) Ch %i - %02x%02x%02x%02x%02x%02x", bss->ssid, bss->rssi, bss->channel, bss->bssid[0], bss->bssid[1], bss->bssid[2], bss->bssid[3], bss->bssid[4], bss->bssid[5]);

        wifi_network_info_t* net = context->wifi_networks;
        while (net) {
            if (!memcmp(net->bssid, bss->bssid, 6)) {
                break;
            }
            net = net->next;
        }
        
        if (!net) {
            wifi_network_info_t *net = malloc(sizeof(wifi_network_info_t));
            memset(net, 0, sizeof(*net));
            strncpy(net->ssid, (char *)bss->ssid, sizeof(net->ssid) - 1);
            memcpy(net->bssid, bss->bssid, 6);
            itoa(bss->rssi, net->rssi, 10);
            itoa(bss->channel, net->channel, 10);
            net->secure = bss->authmode != AUTH_OPEN;
            net->next = context->wifi_networks;

            context->wifi_networks = net;
        }

        bss = bss->next.stqe_next;
    }

    xSemaphoreGive(context->wifi_networks_semaph);
}
#endif

static void wifi_scan_task(void *arg) {
    INFO("Start scan");
    
#ifdef ESP_PLATFORM
    if (wifi_config_get_ip() < 0) {
        sdk_wifi_station_disconnect();
    }
    
    if (esp_wifi_scan_start(NULL, true) == ESP_OK) {
        wifi_scan_done_cb();
    }
    
    if (wifi_config_get_ip() < 0) {
        sdk_wifi_station_connect();
    }
#else
    sdk_wifi_station_scan(NULL, wifi_scan_done_cb);
#endif
    
    vTaskDelete(NULL);
}

static void setup_announcer_task() {
    const struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_DGRAM,
    };
    
    while (getaddrinfo(SETUP_ANNOUNCER_DESTINATION, SETUP_ANNOUNCER_PORT, &hints, &context->res) != 0) {
        vTaskDelay(MS_TO_TICKS(200));
    }

    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);
    char message[13];
    snprintf(message, sizeof(message), "HAA-%02X%02X%02X\r\n", macaddr[3], macaddr[4], macaddr[5]);
    
    for (;;) {
        int s = socket(context->res->ai_family, context->res->ai_socktype, 0);
        if (s >= 0) {
            lwip_sendto(s, message, strlen(message), 0, context->res->ai_addr, context->res->ai_addrlen);
            lwip_close(s);
        }
        
        vTaskDelay(MS_TO_TICKS(3000));
    }
}

#include "setup.html.h"

static void wifi_config_server_on_settings(client_t *client) {
    rs_esp_timer_change_period_forced(context->auto_reboot_timer, AUTO_REBOOT_LONG_TIMEOUT);
    
    xTaskCreate(wifi_scan_task, "SCA", (TASK_SIZE_FACTOR * 384), NULL, (tskIDLE_PRIORITY + 2), NULL);
    
    static const char http_prologue[] =
        "HTTP/1.1 200 \r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Cache-Control: no-store\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Connection: close\r\n"
        "\r\n";
    
    client_send(client, http_prologue, sizeof(http_prologue) - 1);
    client_send_chunk(client, html_settings_header);
    
    char *text = NULL;
    sysparam_get_string(INSTALLER_VERSION_SYSPARAM, &text);
    if (text) {
        client_send_chunk(client, text);
        free(text);
        text = NULL;
    } else {
        client_send_chunk(client, "NONE");
    }
    client_send_chunk(client, html_settings_installer_version);
    
    sysparam_status_t status = sysparam_get_string(HAA_SCRIPT_SYSPARAM, &text);
    if (status == SYSPARAM_OK) {
        client_send_chunk(client, text);
        free(text);
        text = NULL;
    }
    client_send_chunk(client, html_settings_middle);
    
    if (context->param >= 100) {
        context->param -= 100;
        client_send_chunk(client, "! Script<br>");
    }
    
    if (context->param == 1) {
        client_send_chunk(client, "! Flash<br>");
    }
    
    client_send_chunk(client, html_settings_pairings);
    
    const int homekit_pairings = homekit_pairing_count();
    char homekit_pairings_text[4];
    itoa(homekit_pairings, homekit_pairings_text, 10);
    
    client_send_chunk(client, homekit_pairings_text);
    
    client_send_chunk(client, html_settings_pairings_count);
    
    if (homekit_pairings > 0 && homekit_pairings < 40) {
        client_send_chunk(client, html_settings_add_extra_pairing);
    }
    
    int8_t int8_value = 0;
    sysparam_get_int8(HOMEKIT_PAIRING_COUNT_SYSPARAM, &int8_value);
    if (int8_value > 1 && homekit_pairings > int8_value) {
        client_send_chunk(client, html_settings_remove_extra_pairing);
    }
    
    client_send_chunk(client, html_settings_reset_homekit_id);
    
    int8_value = 0;
    sysparam_get_int8(WIFI_MODE_SYSPARAM, &int8_value);
    if (int8_value == 0) {
        client_send_chunk(client, "selected");
    }
    client_send_chunk(client, html_wifi_mode_0);
    
    if (int8_value == 1) {
        client_send_chunk(client, "selected");
    }
    client_send_chunk(client, html_wifi_mode_1);
    
    if (int8_value == 2) {
        client_send_chunk(client, "selected");
    }
    client_send_chunk(client, html_wifi_mode_2);
    
    if (int8_value == 3) {
        client_send_chunk(client, "selected");
    }
    client_send_chunk(client, html_wifi_mode_3);
    
    if (int8_value == 4) {
        client_send_chunk(client, "selected");
    }
    client_send_chunk(client, html_wifi_mode_4);
    
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
                net->secure ? "secure" : "unsecure", bssid, net->ssid, net->ssid, net->rssi, net->channel, bssid
            );
            client_send_chunk(client, buffer);
            
            net = net->next;
        }

        xSemaphoreGive(context->wifi_networks_semaph);
    }

    client_send_chunk(client, html_settings_footer);

    client_send_chunk(client, "");
}

static void wifi_config_context_free(wifi_config_context_t* context) {
    if (context->ssid_prefix) {
        free(context->ssid_prefix);
    }

    wifi_networks_free();

    free(context);
    context = NULL;
}

static void wifi_config_server_on_settings_update_task(void* args) {
    client_t* client = args;
    context->end_setup = true;
    
    while (context->end_setup) {
        vTaskDelay(MS_TO_TICKS(500));
    }
    
    sdk_wifi_station_disconnect();
    wifi_config_context_free(context);
    
    INFO("New settings");
    
    form_param_t *form = form_params_parse((char*) client->body);
    client_free(client);
    
    if (!form) {
        ERROR("DRAM");
        
    } else {
        form_param_t *reset_sys_param = form_params_find(form, "rsy");
        form_param_t *reset_hk_param = form_params_find(form, "rst");
        
        if (reset_sys_param) {
            int32_t last_config_number = 0;
            if (reset_hk_param) {
                homekit_server_reset();
                last_config_number = 1;
            } else {
                sysparam_get_int32(LAST_CONFIG_NUMBER_SYSPARAM, &last_config_number);
            }
            
            char* installer_version_string = NULL;
            sysparam_get_string(INSTALLER_VERSION_SYSPARAM, &installer_version_string);
            
            char* haamain_version_string = NULL;
            sysparam_get_string(HAAMAIN_VERSION_SYSPARAM, &haamain_version_string);
            
            int8_t saved_pairing_count = -1;
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
            
            if (saved_pairing_count > -1) {
                sysparam_set_int8(HOMEKIT_PAIRING_COUNT_SYSPARAM, saved_pairing_count);
            }
            
        } else {
            form_param_t *conf_param = form_params_find(form, "cnf");
            form_param_t *re_pair_param = form_params_find(form, "rep");
            form_param_t *rm_re_pair_param = form_params_find(form, "rmp");
            form_param_t *nowifi_param = form_params_find(form, "now");
            form_param_t *ota_param = form_params_find(form, "ota");
            form_param_t *installer_setup_param = form_params_find(form, "ins");
            form_param_t *wifimode_param = form_params_find(form, "wm");
            form_param_t *irrx_param = form_params_find(form, "irx");
            form_param_t *ssid_param = form_params_find(form, "sid");
            form_param_t *bssid_param = form_params_find(form, "bid");
            form_param_t *password_param = form_params_find(form, "psw");
            
            // Remove saved states
            int32_t hk_total_serv = 0;
            sysparam_get_int32(TOTAL_SERV_SYSPARAM, &hk_total_serv);
            
            char saved_state_id[8];
            for (int serv = 1; serv <= hk_total_serv; serv++) {
                for (int ch = 0; ch <= HIGH_HOMEKIT_CH_NUMBER; ch++) {
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
            
            if (re_pair_param) {
                const int8_t re_pair_int = 1;
                sysparam_set_int8(HOMEKIT_RE_PAIR_SYSPARAM, re_pair_int);
                
                int8_t count = 0;
                sysparam_get_int8(HOMEKIT_PAIRING_COUNT_SYSPARAM, &count);
                if (count == 0) {
                    count = homekit_pairing_count();
                    sysparam_set_int8(HOMEKIT_PAIRING_COUNT_SYSPARAM, count);
                }
            } else {
                sysparam_erase(HOMEKIT_RE_PAIR_SYSPARAM);
            }
            
            if (rm_re_pair_param) {
                int8_t count = 2;
                sysparam_get_int8(HOMEKIT_PAIRING_COUNT_SYSPARAM, &count);
                homekit_remove_extra_pairing(count);
                sysparam_erase(HOMEKIT_PAIRING_COUNT_SYSPARAM);
            }
            
            if (ota_param || installer_setup_param) {
#ifdef ESP_PLATFORM
                setup_set_boot_installer();
#else
                rboot_set_temp_rom(1);
#endif
                
                if (installer_setup_param) {
                    sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 1);
                }
            } else if (irrx_param && irrx_param->value) {
                const uint8_t irrx_gpio = (strtol(irrx_param->value, NULL, 10)) + 100;
                if (irrx_gpio >= 100) {     // If not GPIO is selected, irrx_gpio == 99
                    sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, irrx_gpio);
                }
            }
            
            if (nowifi_param) {
                sysparam_erase(WIFI_SSID_SYSPARAM);
                sysparam_erase(WIFI_PASSWORD_SYSPARAM);
                sysparam_erase(WIFI_BSSID_SYSPARAM);
                sysparam_erase(WIFI_MODE_SYSPARAM);
#ifndef ESP_PLATFORM
                sysparam_erase(WIFI_LAST_WORKING_PHY_SYSPARAM);
#endif
            }
            
            int32_t last_config_number = 0;
            sysparam_get_int32(LAST_CONFIG_NUMBER_SYSPARAM, &last_config_number);
            last_config_number++;
            if (last_config_number > 65535) {
                last_config_number = 1;
            }
            
            if (reset_hk_param) {
                homekit_server_reset();
                sysparam_erase(HOMEKIT_RE_PAIR_SYSPARAM);
                last_config_number = 1;
            }
            sysparam_set_int32(LAST_CONFIG_NUMBER_SYSPARAM, last_config_number);
            
            if (ssid_param && ssid_param->value) {
                sysparam_set_string(WIFI_SSID_SYSPARAM, ssid_param->value);
                
                if (bssid_param && bssid_param->value && strlen(bssid_param->value) == 12) {
                    uint8_t bssid[6];
                    char hex[3];
                    hex[2] = 0;
                    
                    for (int i = 0; i < 6; i++) {
                        hex[0] = bssid_param->value[(i * 2)];
                        hex[1] = bssid_param->value[(i * 2) + 1];
                        bssid[i] = (uint8_t) strtol(hex, NULL, 16);
                    }
                    
                    sysparam_set_blob(WIFI_BSSID_SYSPARAM, bssid, (size_t) 6);
                    
                } else {
                    sysparam_erase(WIFI_BSSID_SYSPARAM);
                }
                
                if (password_param->value) {
                    sysparam_set_string(WIFI_PASSWORD_SYSPARAM, password_param->value);
                } else {
                    sysparam_set_string(WIFI_PASSWORD_SYSPARAM, "");
                }
            }
            
            if (wifimode_param && wifimode_param->value) {
                int8_t current_wifi_mode = 0;
                int8_t new_wifi_mode = strtol(wifimode_param->value, NULL, 10);
                sysparam_get_int8(WIFI_MODE_SYSPARAM, &current_wifi_mode);
                sysparam_set_int8(WIFI_MODE_SYSPARAM, new_wifi_mode);
            }
            
#ifndef ESP_PLATFORM
            wifi_config_reset();
            vTaskDelay(MS_TO_TICKS(5000));
#endif
            
        }
    }
    
    INFO("Reboot");
    vTaskDelay(MS_TO_TICKS(1000));
    sdk_system_restart();
}

static int wifi_config_server_on_url(http_parser *parser, const char *data, size_t length) {
    client_t *client = (client_t*) parser->data;

    client->endpoint = ENDPOINT_UNKNOWN;
    if (parser->method == HTTP_GET) {
        if (!strncmp(data, "/st", length)) {
            client->endpoint = ENDPOINT_SETTINGS;
        } else if (!strncmp(data, "/", length)) {
            client->endpoint = ENDPOINT_INDEX;
        }
    } else if (parser->method == HTTP_POST) {
        if (!strncmp(data, "/st", length)) {
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

static int wifi_config_server_on_body(http_parser *parser, const char *data, size_t length) {
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
            client_send_redirect(client, 301, "/st");
            break;
            
        case ENDPOINT_SETTINGS:
            wifi_config_server_on_settings(client);
            break;
        
        case ENDPOINT_SETTINGS_UPDATE:
            rs_esp_timer_change_period_forced(context->auto_reboot_timer, AUTO_REBOOT_LONG_TIMEOUT);
            if (context->sta_connect_timeout) {
                vTaskDelete(context->sta_connect_timeout);
            } else {
                sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 0);
            }
            
            if (context->setup_announcer) {
                vTaskDelete(context->setup_announcer);
                if (context->res) {
                    freeaddrinfo(context->res);
                }
            }
            
            xTaskCreate(wifi_config_server_on_settings_update_task, "UDP", (TASK_SIZE_FACTOR * 512), client, (tskIDLE_PRIORITY + 1), NULL);
            return 0;
        /*
        case ENDPOINT_UNKNOWN:
            INFO("Unknown");
            client_send_redirect(client, 302, "http://192.168.4.1:4567/st");
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
            vTaskDelay(MS_TO_TICKS(1000));
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
    LOCK_TCPIP_CORE();
    sdk_wifi_set_opmode(STATIONAP_MODE);
    UNLOCK_TCPIP_CORE();
    
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);
    
    const uint8_t wifi_channel = ((hwrand() % 3) * 5) + 1;
    
#ifdef ESP_PLATFORM
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
        "%s-%02X%02X%02X", context->ssid_prefix, macaddr[3], macaddr[4], macaddr[5]
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
#else
    struct sdk_softap_config softap_config;
    softap_config.ssid_hidden = 0;
    softap_config.channel = wifi_channel;
    softap_config.authmode = AUTH_OPEN;
    softap_config.max_connection = 2;
    softap_config.beacon_interval = 100;
    
    softap_config.ssid_len = snprintf(
        (char *)softap_config.ssid, sizeof(softap_config.ssid),
        "%s-%02X%02X%02X", context->ssid_prefix, macaddr[3], macaddr[4], macaddr[5]
    );
    
    INFO("Wifi AP %s Ch%i", softap_config.ssid, softap_config.channel);
    
    struct ip_info ap_ip;
    IP4_ADDR(&ap_ip.ip, 192, 168, 4, 1);
    IP4_ADDR(&ap_ip.netmask, 255, 255, 255, 0);
    IP4_ADDR(&ap_ip.gw, 0, 0, 0, 0);
    sdk_wifi_set_ip_info(SOFTAP_IF, &ap_ip);
    
    sdk_wifi_softap_set_config(&softap_config);
    
    ip4_addr_t first_client_ip;
    first_client_ip.addr = ap_ip.ip.addr + htonl(1);
    
    dhcpserver_start(&first_client_ip, 4);
#endif
    
    context->wifi_networks_semaph = xSemaphoreCreateBinary();
    xSemaphoreGive(context->wifi_networks_semaph);

    xTaskCreate(http_task, "WEB", (TASK_SIZE_FACTOR * 640), NULL, (tskIDLE_PRIORITY + 1), NULL);
}

#ifndef ESP_PLATFORM
void save_last_working_phy() {
    int8_t phy_mode = 3;
    if (sdk_wifi_get_phy_mode() == PHY_MODE_11G) {
        phy_mode = 2;
    }
    sysparam_set_int8(WIFI_LAST_WORKING_PHY_SYSPARAM, phy_mode);
}
#endif

static void auto_reboot_run() {
    INFO("Auto Reboot");
    
    if (context->sta_connect_timeout) {
        vTaskDelete(context->sta_connect_timeout);
    }
    
    if (context->setup_announcer) {
        vTaskDelete(context->setup_announcer);
        if (context->res) {
            freeaddrinfo(context->res);
        }
    }
    
    vTaskDelay(MS_TO_TICKS(500));
    
    sdk_system_restart();
}

static void wifi_config_sta_connect_timeout_task() {
#ifndef ESP_PLATFORM
    struct netif* netif = NULL;
    for (;;) {
        netif = sdk_system_get_netif(STATION_IF);
        if (netif) {
            break;
        }
        vTaskDelay(MS_TO_TICKS(100));
    }
    
    if (context->on_wifi_ready && context->custom_hostname) {
        netif->hostname = context->custom_hostname;
    } else {
        uint8_t macaddr[6];
        sdk_wifi_get_macaddr(STATION_IF, macaddr);
        char* setup_hostname = malloc(17);
        snprintf(setup_hostname, 17, "HAA-%02X%02X%02X-Setup", macaddr[3], macaddr[4], macaddr[5]);
        netif->hostname = setup_hostname;
    }
#endif
    
    const int is_station_mode = (sdk_wifi_get_opmode() == STATION_MODE);
    
    for (;;) {
        vTaskDelay(MS_TO_TICKS(1000));
#ifdef ESP_PLATFORM
        if (wifi_config_get_ip() >= 0) {
#else
        if (sdk_wifi_station_get_connect_status() == STATION_GOT_IP) {
            save_last_working_phy();
#endif
            if (context->on_wifi_ready) {
                context->on_wifi_ready();
                
                wifi_config_context_free(context);
                
            } else {
                LOCK_TCPIP_CORE();
#ifdef ESP_PLATFORM
                esp_netif_dhcps_stop(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"));
#else
                dhcpserver_stop();
#endif
                sdk_wifi_set_opmode(STATION_MODE);
                UNLOCK_TCPIP_CORE();
                
                xTaskCreate(setup_announcer_task, "STA", (TASK_SIZE_FACTOR * 512), NULL, (tskIDLE_PRIORITY + 0), &context->setup_announcer);
            }
            
            break;
            
        } else if (is_station_mode) {
            context->check_counter++;
            if (context->check_counter > 35) {
                context->check_counter = 0;
#ifdef ESP_PLATFORM
                wifi_config_connect(0, true);
#else
                wifi_config_connect(0, 4, true);
#endif
                
                vTaskDelay(MS_TO_TICKS(1000));
            }
        }
    }
    
    if (context) {
        context->sta_connect_timeout = NULL;
    }
    vTaskDelete(NULL);
}

#ifdef ESP_PLATFORM   
static void free_wifi_config_ip_info() {
    ip_addr.addr = 0;
    gw_addr.addr = 0;
}
    
static void on_got_ip(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    free_wifi_config_ip_info();
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ip_addr.addr = event->ip_info.ip.addr;
    gw_addr.addr = event->ip_info.gw.addr;
    INFO("Wifi connected. ip:" IPSTR " gw:" IPSTR, IP2STR(&ip_addr), IP2STR(&gw_addr));
    
    char* buf = malloc(16);
    snprintf(buf, 16, IPSTR, IP2STR(&event->ip_info.ip));
    adv_logger_set_ip_address(buf);
}
        
static void on_disconnect(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    ERROR("Wifi disconnected");
    free_wifi_config_ip_info();
    sdk_wifi_station_connect();
}

uint8_t wifi_config_connect(const uint8_t mode, const bool with_reset) {
#else
uint8_t wifi_config_connect(const uint8_t mode, const uint8_t phy, const bool with_reset) {
#endif
    char *wifi_ssid = NULL;
    sysparam_get_string(WIFI_SSID_SYSPARAM, &wifi_ssid);
    
#ifdef ESP_PLATFORM
    wifi_config_t sta_config = {
        .sta = {
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            .bssid_set = 0,
        },
    };
    
    sdk_wifi_station_set_config(&sta_config);
    
    if (!context->bandwidth_40) {
        esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
    }
    
    if (context->wifi_sleep_mode == 0) {
        esp_wifi_set_ps(WIFI_PS_NONE);
    } else {
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    }
#endif
    
    if (wifi_ssid) {
        if (with_reset) {
            wifi_config_reset();
            vTaskDelay(MS_TO_TICKS(5000));
        }
        
        sdk_wifi_station_disconnect();
        
        char *wifi_password = NULL;
        sysparam_get_string(WIFI_PASSWORD_SYSPARAM, &wifi_password);
        
#ifdef ESP_PLATFORM
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL);
        esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_disconnect, NULL);
        
        strncpy((char *)sta_config.sta.ssid, wifi_ssid, sizeof(sta_config.sta.ssid) - 1);
        
        if (wifi_password) {
           strncpy((char *)sta_config.sta.password, wifi_password, sizeof(sta_config.sta.password) - 1);
        }
#else
        struct sdk_station_config sta_config;
        memset(&sta_config, 0, sizeof(sta_config));
        
        strncpy((char *)sta_config.ssid, wifi_ssid, sizeof(sta_config.ssid) - 1);
        
        if (wifi_password) {
           strncpy((char *)sta_config.password, wifi_password, sizeof(sta_config.password) - 1);
        }
#endif

        int8_t wifi_mode = 0;
        sysparam_get_int8(WIFI_MODE_SYSPARAM, &wifi_mode);
        
        uint8_t *wifi_bssid = NULL;
        size_t len = 6;
        sysparam_get_blob(WIFI_BSSID_SYSPARAM, &wifi_bssid, &len);
        
        INFO_NNL("BSSID ");
        if (wifi_bssid) {
            INFO("%02x%02x%02x%02x%02x%02x", wifi_bssid[0], wifi_bssid[1], wifi_bssid[2], wifi_bssid[3], wifi_bssid[4], wifi_bssid[5]);
        } else {
            INFO("-");
        }
        
        INFO_NNL("Mode ");
        if (wifi_mode < 2 || (wifi_mode == 4 && mode == 1)) {
            if ((wifi_mode == 1 || (wifi_mode == 4 && mode == 1)) && wifi_bssid) {
#ifdef ESP_PLATFORM
                sta_config.sta.bssid_set = 1;
                memcpy(sta_config.sta.bssid, wifi_bssid, 6);
#else
                sta_config.bssid_set = 1;
                memcpy(sta_config.bssid, wifi_bssid, 6);
#endif
                INFO("Forced");
                
                //INFO("Wifi Mode: Forced BSSID %02x%02x%02x%02x%02x%02x", sta_config.bssid[0], sta_config.bssid[1], sta_config.bssid[2], sta_config.bssid[3], sta_config.bssid[4], sta_config.bssid[5]);

            } else {
                INFO("Normal");
            }
            
            LOCK_TCPIP_CORE();
            sdk_wifi_set_opmode(STATION_MODE);
            UNLOCK_TCPIP_CORE();
            sdk_wifi_station_set_config(&sta_config);
            sdk_wifi_station_set_auto_connect(true);

#ifndef ESP_PLATFORM
            wifi_config_toggle_phy_mode(phy);
#endif
            
            sdk_wifi_station_connect();
            
        } else {
            INFO("Roaming");
            sysparam_erase(WIFI_BSSID_SYSPARAM);
            LOCK_TCPIP_CORE();
            sdk_wifi_set_opmode(STATION_MODE);
            UNLOCK_TCPIP_CORE();
            sdk_wifi_station_set_config(&sta_config);
            sdk_wifi_station_set_auto_connect(true);
            
#ifndef ESP_PLATFORM
            wifi_config_toggle_phy_mode(phy);
#endif
            
            if (wifi_mode == 4) {
                xTaskCreate(wifi_scan_sc_task, "SMA", (TASK_SIZE_FACTOR * 384), NULL, (tskIDLE_PRIORITY + 2), NULL);
            } else {
                wifi_config_smart_connect();
            }
        }
        
        free(wifi_ssid);
        
        if (wifi_password) {
            free(wifi_password);
        }
        
        if (wifi_bssid) {
            free(wifi_bssid);
        }
        
        return 1;
    }
    
    INFO("No Wifi config");
    return 0;
}

static void wifi_config_station_connect() {
    vTaskDelay(1);
    
#ifndef ESP_PLATFORM
    int8_t phy_mode = 3;
    if (!context->on_wifi_ready) {
        sysparam_get_int8(WIFI_LAST_WORKING_PHY_SYSPARAM, &phy_mode);
    }
#endif

#ifdef ESP_PLATFORM
    if (wifi_config_connect(0, false) == 1) {
#else
    if (wifi_config_connect(0, phy_mode, false) == 1) {
#endif
        xTaskCreate(wifi_config_sta_connect_timeout_task, "STI", (TASK_SIZE_FACTOR * 640), NULL, (tskIDLE_PRIORITY + 1), &context->sta_connect_timeout);
        
        if (!context->on_wifi_ready) {
            INFO("HAA Setup");
            
            int8_t setup_mode = 0;
            sysparam_get_int8(HAA_SETUP_MODE_SYSPARAM, &setup_mode);
            
            sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 0);
            
            if (setup_mode == 1) {
                INFO("Enabling auto reboot");
                context->auto_reboot_timer = rs_esp_timer_create(AUTO_REBOOT_TIMEOUT, false, NULL, auto_reboot_run);
                rs_esp_timer_start_forced(context->auto_reboot_timer);
            } else if (setup_mode == 2) {
                ERROR("Script");
                context->param += 100;
            }
            
            wifi_config_softap_start();
        }
        
    } else {
        wifi_config_softap_start();
    }
    
    vTaskDelete(NULL);
}

#ifdef ESP_PLATFORM
void wifi_config_init(const char* ssid_prefix, void (*on_wifi_ready)(), const char* custom_hostname, const int param, const uint8_t wifi_sleep_mode, const bool bandwidth_40) {
#else
void wifi_config_init(const char* ssid_prefix, void (*on_wifi_ready)(), const char* custom_hostname, const int param) {
#endif
    INFO("Wifi init");
    
    context = malloc(sizeof(wifi_config_context_t));
    memset(context, 0, sizeof(*context));
    
    context->ssid_prefix = strndup(ssid_prefix, 33 - 7);
    
#ifdef ESP_PLATFORM
    context->wifi_sleep_mode = wifi_sleep_mode;
    context->bandwidth_40 = bandwidth_40;
    
    if (on_wifi_ready && custom_hostname) {
        esp_netif_set_hostname(setup_esp_netif, strdup(custom_hostname));
    } else {
        uint8_t macaddr[6];
        sdk_wifi_get_macaddr(STATION_IF, macaddr);
        char* setup_hostname = malloc(17);
        snprintf(setup_hostname, 17, "HAA-%02X%02X%02X-Setup", macaddr[3], macaddr[4], macaddr[5]);
        esp_netif_set_hostname(setup_esp_netif, setup_hostname);
    }
#else
    if (custom_hostname) {
        context->custom_hostname = strdup(custom_hostname);
    }
#endif

    context->on_wifi_ready = on_wifi_ready;
    context->param = param;

    xTaskCreate(wifi_config_station_connect, "WCO", (TASK_SIZE_FACTOR * 512), NULL, (tskIDLE_PRIORITY + 1), NULL);
}
