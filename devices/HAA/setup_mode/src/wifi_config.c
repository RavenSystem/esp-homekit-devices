/*
 * Home Accessory Architect
 *
 * Copyright 2019-2020 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

/*
 * Based on esp-wifi-config library by Maxim Kulkin (@MaximKulkin), licensed under the MIT License.
 * https://github.com/maximkulkin/esp-wifi-config
 */

#include <stdio.h>
#include <string.h>
#include <sysparam.h>
#include <FreeRTOS.h>
#include <esplibs/libmain.h>
#include <espressif/esp_common.h>
#include <lwip/sockets.h>
#include <lwip/dhcp.h>
#include <spiflash.h>

#include <semphr.h>

#include <http-parser/http_parser.h>
#include <dhcpserver.h>

#include "form_urlencoded.h"

#include <rboot-api.h>
#include <homekit/homekit.h>

#include <timers_helper.h>

#define CUSTOM_REPO_SYSPARAM            "ota_sever"
#define PORT_NUMBER_SYSPARAM            "ota_port"
#define PORT_SECURE_SYSPARAM            "ota_sec"
#define OTA_VERSION_SYSPARAM            "ota_repo"
#define WIFI_SSID_SYSPARAM              "wifi_ssid"
#define WIFI_PASSWORD_SYSPARAM          "wifi_password"
#define WIFI_MODE_SYSPARAM              "wifi_mode"
#define WIFI_BSSID_SYSPARAM             "wifi_bssid"
#define AUTO_OTA_SYSPARAM               "aota"
#define TOTAL_ACC_SYSPARAM              "total_ac"
#define HAA_JSON_SYSPARAM               "haa_conf"
#define HAA_SETUP_MODE_SYSPARAM         "setup"
#define LAST_CONFIG_NUMBER              "hkcf"

// Sysparam
#define SYSPARAMSECTOR                  (0xF3000)
#define SYSPARAMSIZE                    (8)
#define SECTORSIZE                      (4096)

#define WIFI_CONFIG_SERVER_PORT         (80)

#define AUTO_REBOOT_TIMEOUT             (90000)

#define MAX_BODY_LEN                    (16000)

#define INFO(message, ...)              printf(message "\n", ##__VA_ARGS__);
#define ERROR(message, ...)             printf("! " message "\n", ##__VA_ARGS__);

typedef enum {
    ENDPOINT_UNKNOWN = 0,
    ENDPOINT_INDEX,
    ENDPOINT_SETTINGS,
    ENDPOINT_SETTINGS_UPDATE,
} endpoint_t;

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
    char* password;
    char* custom_hostname;
    void (*on_wifi_ready)();

    TimerHandle_t auto_reboot_timer;
    
    TaskHandle_t http_task_handle;
    TaskHandle_t dns_task_handle;
    TaskHandle_t sta_connect_timeout;
    
    wifi_network_info_t* wifi_networks;
    SemaphoreHandle_t wifi_networks_mutex;

    uint8_t check_counter: 6;
    bool hostname_ready: 1;
} wifi_config_context_t;

static wifi_config_context_t* context;

typedef struct _client {
    int fd;

    http_parser parser;
    endpoint_t endpoint;
    uint8_t *body;
    size_t body_length;
} client_t;

static void wifi_config_station_connect();
static void wifi_config_softap_start();
static void wifi_config_softap_stop();

static void body_malloc(client_t* client) {
    uint16_t body_size = MAX_BODY_LEN;
    do {
        client->body = malloc(body_size);
        body_size -= 200;
    } while (!client->body);
}

static client_t *client_new() {
    client_t *client = malloc(sizeof(client_t));
    if (client == NULL) {
        return;
    }
    memset(client, 0, sizeof(client_t));
    
    body_malloc(client);

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

static void client_send(client_t *client, const char *payload, size_t payload_size) {
    lwip_write(client->fd, payload, payload_size);
}

static void client_send_chunk(client_t *client, const char *payload) {
    int len = strlen(payload);
    char buffer[10];
    int buffer_len = snprintf(buffer, sizeof(buffer), "%x\r\n", len);
    client_send(client, buffer, buffer_len);
    client_send(client, payload, len);
    client_send(client, "\r\n", 2);
}

static void client_send_redirect(client_t *client, int code, const char *redirect_url) {
    INFO("Redirecting to %s", redirect_url);
    char buffer[128];
    size_t len = snprintf(buffer, sizeof(buffer), "HTTP/1.1 %d \r\nLocation: %s\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", code, redirect_url);
    client_send(client, buffer, len);
}

uint8_t wifi_config_connect();
void wifi_config_reset() {
    struct sdk_station_config sta_config;
    memset(&sta_config, 0, sizeof(sta_config));

    strncpy((char *)sta_config.ssid, "none", sizeof(sta_config.ssid));
    sta_config.ssid[sizeof(sta_config.ssid) - 1] = 0;
    sdk_wifi_station_set_config(&sta_config);
    sdk_wifi_station_connect();
    sdk_wifi_station_set_auto_connect(false);
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

static void wifi_scan_done_cb(void *arg, sdk_scan_status_t status) {
    if (status != SCAN_OK || !context) {
        ERROR("Wifi scan failed");
        return;
    }

    xSemaphoreTake(context->wifi_networks_mutex, portMAX_DELAY);

    if (!context) {
        ERROR("No context");
        return;
    }
    
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
            if (net == NULL) {
                return;
            }
            memset(net, 0, sizeof(*net));
            strncpy(net->ssid, (char *)bss->ssid, sizeof(net->ssid));
            memcpy(net->bssid, bss->bssid, 6);
            itoa(bss->rssi, net->rssi, 10);
            itoa(bss->channel, net->channel, 10);
            net->secure = bss->authmode != AUTH_OPEN;
            net->next = context->wifi_networks;

            context->wifi_networks = net;
        }

        bss = bss->next.stqe_next;
    }

    xSemaphoreGive(context->wifi_networks_mutex);
}

static void wifi_scan_task(void *arg) {
    INFO("Start Wifi scan");
    sdk_wifi_station_scan(NULL, wifi_scan_done_cb);
    vTaskDelete(NULL);
}

#include "index.html.h"

static void wifi_config_server_on_settings(client_t *client) {
    esp_timer_delete(context->auto_reboot_timer);
    
    xTaskCreate(wifi_scan_task, "wifi_scan", 384, NULL, (tskIDLE_PRIORITY + 0), NULL);
    
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
    sysparam_status_t status;
    status = sysparam_get_string(HAA_JSON_SYSPARAM, &text);
    if (status == SYSPARAM_OK) {
        client_send_chunk(client, text);
        free(text);
        text = NULL;
    }
    client_send_chunk(client, html_settings_middle);
    
    status = sysparam_get_string(OTA_VERSION_SYSPARAM, &text);
    if (text) {
        client_send_chunk(client, text);
        free(text);
        text = NULL;
    }
    client_send_chunk(client, html_settings_otaversion);
    
    bool auto_ota = false;
    status = sysparam_get_bool(AUTO_OTA_SYSPARAM, &auto_ota);
    if (status == SYSPARAM_OK && auto_ota) {
        client_send_chunk(client, "checked");
    }
    client_send_chunk(client, html_settings_autoota);
    
    int8_t int8_value = 0;
    sysparam_get_int8(WIFI_MODE_SYSPARAM, &int8_value);
    if (int8_value == 0) {
        client_send_chunk(client, "selected");
    }
    client_send_chunk(client, html_wifi_mode_0);
    
    if (int8_value == 1) {
        client_send_chunk(client, "selected");
    }
    client_send_chunk(client, html_wifi_mode_1);
    
    // Wifi Networks
    char buffer[120];
    char bssid[13];
    if (xSemaphoreTake(context->wifi_networks_mutex, pdMS_TO_TICKS(5000))) {
        wifi_network_info_t* net = context->wifi_networks;
        while (net) {
            snprintf(bssid, 13, "%02x%02x%02x%02x%02x%02x", net->bssid[0], net->bssid[1], net->bssid[2], net->bssid[3], net->bssid[4], net->bssid[5]);
            snprintf(
                buffer, sizeof(buffer),
                html_network_item,
                net->secure ? "secure" : "unsecure", bssid, net->ssid, net->ssid, net->rssi, net->channel, bssid
            );
            client_send_chunk(client, buffer);

            net = net->next;
        }

        xSemaphoreGive(context->wifi_networks_mutex);
    }

    client_send_chunk(client, html_settings_wifi);
    
    // Custom repo server
    status = sysparam_get_string(CUSTOM_REPO_SYSPARAM, &text);
    if (status == SYSPARAM_OK) {
        client_send_chunk(client, text);
        free(text);
    }
    client_send_chunk(client, html_settings_reposerver);
    
    int32_t int32_value;
    
    status = sysparam_get_int32(PORT_NUMBER_SYSPARAM, &int32_value);
    if (status == SYSPARAM_OK) {
        char str_port[6];
        itoa(int32_value, str_port, 10);
        client_send_chunk(client, str_port);
    }
    client_send_chunk(client, html_settings_repoport);
    
    status = sysparam_get_int8(PORT_SECURE_SYSPARAM, &int8_value);
    if (status != SYSPARAM_OK || (status == SYSPARAM_OK && int8_value == 1)) {
        client_send_chunk(client, "checked");
    }
    client_send_chunk(client, html_settings_repossl);

    client_send_chunk(client, "");
}

static void http_stop() {
    if (!context->http_task_handle) {
        return;
    }

    xTaskNotify(context->http_task_handle, 1, eSetValueWithOverwrite);
}

static void wifi_config_context_free(wifi_config_context_t *context) {
    if (context->ssid_prefix) {
        free(context->ssid_prefix);
    }

    if (context->password) {
        free(context->password);
    }

    wifi_networks_free();

    free(context);
    context = NULL;
}

static void wifi_config_server_on_settings_update_task(void* args) {
    INFO("Update settings");
    
    client_t* client = args;

    vTaskDelay(pdMS_TO_TICKS(100));

    form_param_t *form = form_params_parse((char *) client->body);
    free(client->body);
    
    if (!form) {
        ERROR("No enough memory");
        body_malloc(client);
        client->body_length = 0;
        client_send_redirect(client, 302, "/settings");
        return;
    }
    
    form_param_t *conf_param = form_params_find(form, "conf");
    form_param_t *reset_param = form_params_find(form, "reset");
    form_param_t *reset_sys_param = form_params_find(form, "reset_sys");
    form_param_t *nowifi_param = form_params_find(form, "nowifi");
    form_param_t *ota_param = form_params_find(form, "ota");
    form_param_t *autoota_param = form_params_find(form, "autoota");
    form_param_t *wifimode_param = form_params_find(form, "wifimode");
    form_param_t *ssid_param = form_params_find(form, "ssid");
    form_param_t *bssid_param = form_params_find(form, "bssid");
    form_param_t *password_param = form_params_find(form, "password");
    form_param_t *reposerver_param = form_params_find(form, "reposerver");
    form_param_t *repoport_param = form_params_find(form, "repoport");
    form_param_t *repossl_param = form_params_find(form, "repossl");
    
    // Remove saved states
    int8_t hk_total_ac = 0;
    sysparam_get_int8(TOTAL_ACC_SYSPARAM, &hk_total_ac);
    char saved_state_id[5];
    memset(saved_state_id, 0, 5);
    for (uint16_t int_saved_state_id = 100; int_saved_state_id <= hk_total_ac * 100; int_saved_state_id++) {
        itoa(int_saved_state_id, saved_state_id, 10);
        sysparam_set_data(saved_state_id, NULL, 0, false);
    }
    
    if (conf_param && conf_param->value) {
        sysparam_set_string(HAA_JSON_SYSPARAM, conf_param->value);
    }
    
    if (autoota_param) {
        sysparam_set_bool(AUTO_OTA_SYSPARAM, true);
    } else {
        sysparam_set_bool(AUTO_OTA_SYSPARAM, false);
    }
    
    if (ota_param) {
        rboot_set_temp_rom(1);
    }
    
    if (nowifi_param) {
        sysparam_set_data(WIFI_SSID_SYSPARAM, NULL, 0, false);
        sysparam_set_data(WIFI_PASSWORD_SYSPARAM, NULL, 0, false);
    }
    
    int last_config_number = 0;
    sysparam_get_int32(LAST_CONFIG_NUMBER, &last_config_number);
    last_config_number++;
    if (last_config_number > 65535) {
        last_config_number = 1;
    }
    
    if (reset_param) {
        homekit_server_reset();
        last_config_number = 1;
    }
    
    sysparam_set_int32(LAST_CONFIG_NUMBER, last_config_number);
    
    if (reposerver_param && reposerver_param->value) {
        sysparam_set_string(CUSTOM_REPO_SYSPARAM, reposerver_param->value);
    } else {
        sysparam_set_string(CUSTOM_REPO_SYSPARAM, "");
    }
    
    if (repoport_param && repoport_param->value) {
        const int32_t port = strtol(repoport_param->value, NULL, 10);
        sysparam_set_int32(PORT_NUMBER_SYSPARAM, port);
    }
    
    if (repossl_param) {
        sysparam_set_int8(PORT_SECURE_SYSPARAM, 1);
    } else {
        sysparam_set_int8(PORT_SECURE_SYSPARAM, 0);
    }
    
    if (ssid_param && ssid_param->value) {
        sysparam_set_string(WIFI_SSID_SYSPARAM, ssid_param->value);

        if (bssid_param && bssid_param->value && strlen(bssid_param->value) == 12) {
            uint8_t bssid[6];
            char hex[3];
            memset(hex, 0, 3);
            
            for (uint8_t i = 0; i < 6; i++) {
                hex[0] = bssid_param->value[(i * 2)];
                hex[1] = bssid_param->value[(i * 2) + 1];
                bssid[i] = (uint8_t) strtol(hex, NULL, 16);
            }

            sysparam_set_data(WIFI_BSSID_SYSPARAM, bssid, (size_t) 6, true);
            
        } else {
            sysparam_set_data(WIFI_BSSID_SYSPARAM, NULL, 0, true);
        }
        
        if (password_param->value) {
            sysparam_set_string(WIFI_PASSWORD_SYSPARAM, password_param->value);
        } else {
            sysparam_set_string(WIFI_PASSWORD_SYSPARAM, "");
        }
    }
    
    sysparam_compact();
    
    if (wifimode_param && wifimode_param->value) {
        int8_t current_wifi_mode = 0;
        int8_t new_wifi_mode = strtol(wifimode_param->value, NULL, 10);
        sysparam_get_int8(WIFI_MODE_SYSPARAM, &current_wifi_mode);
        sysparam_set_int8(WIFI_MODE_SYSPARAM, new_wifi_mode);
        
        if (current_wifi_mode != new_wifi_mode) {
            wifi_config_reset();
        }
    }
    
    if (reset_sys_param) {
        for (uint8_t i = 0; i < SYSPARAMSIZE; i++) {
            spiflash_erase_sector(SYSPARAMSECTOR + (SECTORSIZE * i));
        }
    }
    
    INFO("\nRebooting...\n\n");
    
    static const char payload[] = "HTTP/1.1 204 \r\nContent-Type: text/html\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    client_send(client, payload, sizeof(payload) - 1);
    
    vTaskDelay(pdMS_TO_TICKS(250));
    
    sdk_system_restart();
}

static int wifi_config_server_on_url(http_parser *parser, const char *data, size_t length) {
    client_t *client = (client_t*) parser->data;

    client->endpoint = ENDPOINT_UNKNOWN;
    if (parser->method == HTTP_GET) {
        if (!strncmp(data, "/settings", length)) {
            client->endpoint = ENDPOINT_SETTINGS;
        } else if (!strncmp(data, "/", length)) {
            client->endpoint = ENDPOINT_INDEX;
        }
    } else if (parser->method == HTTP_POST) {
        if (!strncmp(data, "/settings", length)) {
            client->endpoint = ENDPOINT_SETTINGS_UPDATE;
        }
    }

    if (client->endpoint == ENDPOINT_UNKNOWN) {
        char *url = strndup(data, length);
        ERROR("Unknown: %s %s", http_method_str(parser->method), url);
        free(url);
    }

    return 0;
}

static int wifi_config_server_on_body(http_parser *parser, const char *data, size_t length) {
    client_t *client = parser->data;
    //client->body = realloc(client->body, client->body_length + length + 1);
    memcpy(client->body + client->body_length, data, length);
    client->body_length += length;
    client->body[client->body_length] = 0;

    return 0;
}

static int wifi_config_server_on_message_complete(http_parser *parser) {
    client_t *client = parser->data;

    switch(client->endpoint) {
        case ENDPOINT_INDEX: {
            client_send_redirect(client, 301, "/settings");
            break;
        }
        case ENDPOINT_SETTINGS: {
            wifi_config_server_on_settings(client);
            break;
        }
        case ENDPOINT_SETTINGS_UPDATE: {
            esp_timer_delete(context->auto_reboot_timer);
            if (context->sta_connect_timeout) {
                vTaskDelete(context->sta_connect_timeout);
            } else {
                sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 0);
            }
            wifi_config_context_free(context);
            xTaskCreate(wifi_config_server_on_settings_update_task, "settings_update", 512, client, (tskIDLE_PRIORITY + 0), NULL);
            return 0;
            break;
        }
        case ENDPOINT_UNKNOWN: {
            INFO("Unknown");
            client_send_redirect(client, 302, "http://192.168.4.1/settings");
            break;
        }
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
    INFO("Start HTTP server");

    struct sockaddr_in serv_addr;
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(WIFI_CONFIG_SERVER_PORT);
    int flags;
    if ((flags = lwip_fcntl(listenfd, F_GETFL, 0)) < 0) {
        ERROR("Get HTTP socket flags");
        lwip_close(listenfd);
        vTaskDelete(NULL);
        return;
    };
    if (lwip_fcntl(listenfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        ERROR("Set HTTP socket flags");
        lwip_close(listenfd);
        vTaskDelete(NULL);
        return;
    }
    bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    listen(listenfd, 2);

    char data[128];

    bool running = true;
    while (running) {
        uint32_t task_value = 0;
        if (xTaskNotifyWait(0, 1, &task_value, 0) == pdTRUE) {
            if (task_value) {
                running = false;
                break;
            }
        }

        int fd = accept(listenfd, (struct sockaddr *)NULL, (socklen_t *)NULL);
        if (fd < 0) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        const struct timeval timeout = { 1, 200000 };
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        client_t *client = client_new();
        if (client == NULL) {
            return;
        }
        client->fd = fd;

        //int data_total = 0;
        
        for (;;) {
            int data_len = lwip_read(client->fd, data, sizeof(data));
            
            //data_total += data_len;
            //INFO("lwip_read %d, %d", data_len, data_total);

            if (data_len > 0) {
                taskYIELD();
                http_parser_execute(
                    &client->parser, &wifi_config_http_parser_settings,
                    data, data_len
                );
            } else {
                break;
            }

            if (xTaskNotifyWait(0, 1, &task_value, 0) == pdTRUE) {
                if (task_value) {
                    running = false;
                    break;
                }
            }
        }

        INFO("Client disconnected");

        lwip_close(client->fd);
        client_free(client);
    }

    INFO("Stop HTTP server");

    lwip_close(listenfd);
    vTaskDelete(NULL);
}

static void dns_task(void *arg) {
    INFO("Start DNS server");

    ip4_addr_t server_addr;
    IP4_ADDR(&server_addr, 192, 168, 4, 1);

    struct sockaddr_in serv_addr;
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(53);
    bind(fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

    const struct timeval timeout = { 1, 200000 }; /* 1.2 second timeout */
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    const struct ifreq ifreq1 = { "en1" };
    setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, &ifreq1, sizeof(ifreq1));

    for (;;) {
        char buffer[96];
        struct sockaddr src_addr;
        socklen_t src_addr_len = sizeof(src_addr);
        size_t count = recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr*)&src_addr, &src_addr_len);

        /* Drop messages that are too large to send a response in the buffer */
        if (count > 0 && count <= sizeof(buffer) - 16 && src_addr.sa_family == AF_INET) {
            size_t qname_len = strlen(buffer + 12) + 1;
            uint32_t reply_len = 2 + 10 + qname_len + 16 + 4;

            char *head = buffer + 2;
            *head++ = 0x80; // Flags
            *head++ = 0x00;
            *head++ = 0x00; // Q count
            *head++ = 0x01;
            *head++ = 0x00; // A count
            *head++ = 0x01;
            *head++ = 0x00; // Auth count
            *head++ = 0x00;
            *head++ = 0x00; // Add count
            *head++ = 0x00;
            head += qname_len;
            *head++ = 0x00; // Q type
            *head++ = 0x01;
            *head++ = 0x00; // Q class
            *head++ = 0x01;
            *head++ = 0xC0; // LBL offs
            *head++ = 0x0C;
            *head++ = 0x00; // Type
            *head++ = 0x01;
            *head++ = 0x00; // Class
            *head++ = 0x01;
            *head++ = 0x00; // TTL
            *head++ = 0x00;
            *head++ = 0x00;
            *head++ = 0x78;
            *head++ = 0x00; // RD len
            *head++ = 0x04;
            *head++ = ip4_addr1(&server_addr);
            *head++ = ip4_addr2(&server_addr);
            *head++ = ip4_addr3(&server_addr);
            *head++ = ip4_addr4(&server_addr);

            sendto(fd, buffer, reply_len, 0, &src_addr, src_addr_len);
        }

        uint32_t task_value = 0;
        if (xTaskNotifyWait(0, 1, &task_value, 0) == pdTRUE) {
            if (task_value)
                break;
        }
    }

    INFO("Stop DNS server");

    lwip_close(fd);

    vTaskDelete(NULL);
}

static void dns_stop() {
    if (!context->dns_task_handle)
        return;

    xTaskNotify(context->dns_task_handle, 1, eSetValueWithOverwrite);
}

static void wifi_config_softap_start() {
    sdk_wifi_set_opmode(STATIONAP_MODE);

    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(SOFTAP_IF, macaddr);

    struct sdk_softap_config softap_config;
    softap_config.ssid_len = snprintf(
        (char *)softap_config.ssid, sizeof(softap_config.ssid),
        "%s-%02X%02X%02X", context->ssid_prefix, macaddr[3], macaddr[4], macaddr[5]
    );
    softap_config.ssid_hidden = 0;
    softap_config.channel = 6;
    if (context->password) {
        softap_config.authmode = AUTH_WPA_WPA2_PSK;
        strncpy((char *)softap_config.password,
                context->password, sizeof(softap_config.password));
    } else {
        softap_config.authmode = AUTH_OPEN;
    }
    softap_config.max_connection = 2;
    softap_config.beacon_interval = 100;

    INFO("Start AP SSID=%s", softap_config.ssid);

    struct ip_info ap_ip;
    IP4_ADDR(&ap_ip.ip, 192, 168, 4, 1);
    IP4_ADDR(&ap_ip.netmask, 255, 255, 255, 0);
    IP4_ADDR(&ap_ip.gw, 0, 0, 0, 0);
    sdk_wifi_set_ip_info(SOFTAP_IF, &ap_ip);

    sdk_wifi_softap_set_config(&softap_config);

    ip4_addr_t first_client_ip;
    first_client_ip.addr = ap_ip.ip.addr + htonl(1);

    context->wifi_networks_mutex = xSemaphoreCreateBinary();
    xSemaphoreGive(context->wifi_networks_mutex);

    INFO("Start DHCP server");
    dhcpserver_start(&first_client_ip, 4);
    dhcpserver_set_router(&ap_ip.ip);
    dhcpserver_set_dns(&ap_ip.ip);

    xTaskCreate(dns_task, "dns_task", 384, NULL, (tskIDLE_PRIORITY + 1), &context->dns_task_handle);
    xTaskCreate(http_task, "http_task", 512, NULL, (tskIDLE_PRIORITY + 1), &context->http_task_handle);
}

static void wifi_config_softap_stop() {
    LOCK_TCPIP_CORE();
    dhcpserver_stop();
    dns_stop();
    sdk_wifi_set_opmode(STATION_MODE);
    UNLOCK_TCPIP_CORE();
}

static void wifi_config_sta_connect_timeout_task() {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        if (!context->hostname_ready && context->custom_hostname) {
            struct netif *netif = sdk_system_get_netif(STATION_IF);
            if (netif && !netif->hostname) {
                LOCK_TCPIP_CORE();
                dhcp_release_and_stop(netif);
                netif->hostname = context->custom_hostname;
                INFO("Hostname: %s", context->custom_hostname);
                dhcp_start(netif);
                UNLOCK_TCPIP_CORE();
                context->hostname_ready = true;
            }
        } else if (sdk_wifi_station_get_connect_status() == STATION_GOT_IP) {
            wifi_config_softap_stop();
            
            if (context->on_wifi_ready) {
                http_stop();
                context->on_wifi_ready();
                
                wifi_config_context_free(context);
                break;
            }
        } else if (context->on_wifi_ready) {
            context->check_counter++;
            if (context->check_counter == 60) {
                context->check_counter = 0;
                sdk_wifi_station_disconnect();
                sdk_wifi_station_connect();
            }
        }
    }
    
    vTaskDelete(NULL);
}

static void auto_reboot_run() {
    bool auto_ota = false;
    sysparam_get_bool(AUTO_OTA_SYSPARAM, &auto_ota);
    if (auto_ota) {
        INFO("OTA Update");
        rboot_set_temp_rom(1);
    }

    INFO("Auto Reboot");
    vTaskDelay(pdMS_TO_TICKS(150));
    
    sdk_system_restart();
}

uint8_t wifi_config_connect() {
    char *wifi_ssid = NULL;
    sysparam_get_string(WIFI_SSID_SYSPARAM, &wifi_ssid);
    
    if (wifi_ssid) {
        struct sdk_station_config sta_config;
               
        memset(&sta_config, 0, sizeof(sta_config));
        strncpy((char *)sta_config.ssid, wifi_ssid, sizeof(sta_config.ssid));
        sta_config.ssid[sizeof(sta_config.ssid) - 1] = 0;

        char *wifi_password = NULL;
        sysparam_get_string(WIFI_PASSWORD_SYSPARAM, &wifi_password);
        if (wifi_password) {
           strncpy((char *)sta_config.password, wifi_password, sizeof(sta_config.password));
        }

        int8_t wifi_mode = 0;
        sysparam_get_int8(WIFI_MODE_SYSPARAM, &wifi_mode);
        
        uint8_t *wifi_bssid = NULL;
        size_t len = 6;
        bool is_binary = true;
        sysparam_get_data(WIFI_BSSID_SYSPARAM, &wifi_bssid, &len, &is_binary);
        if (wifi_bssid) {
            INFO("Saved BSSID: %02x%02x%02x%02x%02x%02x", wifi_bssid[0], wifi_bssid[1], wifi_bssid[2], wifi_bssid[3], wifi_bssid[4], wifi_bssid[5]);
        } else {
            INFO("Saved BSSID: none");
        }
        
        if (wifi_mode == 1 && wifi_bssid) {
            sta_config.bssid_set = 1;
            memcpy(sta_config.bssid, wifi_bssid, 6);
            INFO("Wifi Mode: Forced BSSID");
            
            //INFO("Wifi Mode: Forced BSSID %02x%02x%02x%02x%02x%02x", sta_config.bssid[0], sta_config.bssid[1], sta_config.bssid[2], sta_config.bssid[3], sta_config.bssid[4], sta_config.bssid[5]);

        } else {
            INFO("Wifi Mode: Normal");
            sta_config.bssid_set = 0;
        }

        sdk_wifi_station_set_config(&sta_config);
        sdk_wifi_set_opmode(STATION_MODE);
        sdk_wifi_station_connect();
        sdk_wifi_station_set_auto_connect(true);
        
        free(wifi_ssid);
        
        if (wifi_password) {
            free(wifi_password);
        }
        
        if (wifi_bssid) {
            free(wifi_bssid);
        }
        
        return 1;
        
    } else {
        INFO("No Wifi config found");
    }
    
    return 0;
}

static void wifi_config_station_connect() {
    if (wifi_config_connect() == 1) {
        xTaskCreate(wifi_config_sta_connect_timeout_task, "sta_connect", 512, NULL, (tskIDLE_PRIORITY + 0), &context->sta_connect_timeout);
        
        if (!context->on_wifi_ready) {
            INFO("HAA Setup");
            
            int8_t setup_mode = 0;
            sysparam_get_int8(HAA_SETUP_MODE_SYSPARAM, &setup_mode);
            
            sysparam_set_int8(HAA_SETUP_MODE_SYSPARAM, 0);

            if (setup_mode == 1) {
                INFO("Enabling auto reboot");
                context->auto_reboot_timer = esp_timer_create(AUTO_REBOOT_TIMEOUT, false, NULL, auto_reboot_run);
                esp_timer_start(context->auto_reboot_timer);
            }
            
            wifi_config_softap_start();
        }
        
    } else {
        wifi_config_softap_start();
    }
}

void wifi_config_init(const char* ssid_prefix, const char* password, void (*on_wifi_ready)(), const char* custom_hostname) {
    INFO("Wifi config init");
    if (password && strlen(password) < 8) {
        ERROR("Password must be at least 8 characters");
        return;
    }

    context = malloc(sizeof(wifi_config_context_t));
    if (context == NULL) {
        return;
    }
    memset(context, 0, sizeof(*context));

    context->ssid_prefix = strndup(ssid_prefix, 33 - 7);
    if (password) {
        context->password = strdup(password);
    }
    
    if (custom_hostname) {
        context->custom_hostname = strdup(custom_hostname);
        context->hostname_ready = false;
    } else {
        context->hostname_ready = true;
    }

    context->on_wifi_ready = on_wifi_ready;
    context->check_counter = 0;

    wifi_config_station_connect();
}
