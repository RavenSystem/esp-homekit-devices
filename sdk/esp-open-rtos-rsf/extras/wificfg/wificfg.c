/*
 * WiFi configuration via a simple web server.
 *
 * Copyright (C) 2016 OurAirQuality.org
 *
 * Licensed under the Apache License, Version 2.0, January 2004 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *      http://www.apache.org/licenses/
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE CONTRIBUTORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS WITH THE SOFTWARE.
 *
 */

#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include <espressif/esp_common.h>
#include <espressif/user_interface.h>
#include <esp/uart.h>
#include <spiflash.h>
#include <sdk_internal.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <dhcpserver.h>

#include <lwip/api.h>
#include <lwip/init.h>

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "wificfg.h"
#include "sysparam.h"

#if EXTRAS_MDNS_RESPONDER
#include <mdnsresponder.h>
#endif

#if LWIP_MDNS_RESPONDER
#include <lwip/apps/mdns.h>
#endif


const char *wificfg_default_ssid = "EOR_%02X%02X%02X";
const char *wificfg_default_password = "esp-open-rtos";
const char *wificfg_default_hostname = "eor-%02x%02x%02x";

/* The http task stack allocates a single buffer to do much of it's work. */
#define HTTP_BUFFER_SIZE 54

/*
 * Read a line terminated by "\r\n" or "\n" to be robust. Used to read the http
 * status line and headers. On success returns the number of characters read,
 * which might be more that the available buffer size 'len'. Excess characters
 * in a line are discarded as a protection against excessively long lines. On
 * failure -1 is returned. The character case is lowered to give a canonical
 * case for easier comparision. The buffer is null terminated on success, even
 * if truncated.
 */
static int read_crlf_line(int s, char *buf, size_t len)
{
    size_t num = 0;

    do {
        char c;
        ssize_t r = read(s, &c, 1);

        /* Expecting a known terminator so fail on EOF. */
        if (r <= 0)
            return -1;

        if (c == '\n')
            break;

        /* Remove a trailing '\r', and many unexpected characters. */
        if (c < 0x20 || c > 0x7e)
            continue;

        if (num < len)
            buf[num] = tolower((unsigned char)c);

        num++;
    } while(1);

    /* Null terminate. */
    buf[num >= len ? len - 1 : num] = 0;

    return num;
}

ssize_t wificfg_form_name_value(int s, bool *valp, size_t *rem, char *buf, size_t len)
{
    size_t num = 0;

    do {
        if (*rem == 0)
            break;

        char c;
        ssize_t r = read(s, &c, 1);

        /* Expecting a known number of characters so fail on EOF. */
        if (r <= 0) return -1;

        (*rem)--;

        if (valp && c == '=') {
            *valp = true;
            break;
        }

        if (c == '&') {
            if (valp)
                *valp = false;
            break;
        }

        if (num < len)
            buf[num] = c;

        num++;
    } while(1);

    /* Null terminate. */
    buf[num >= len ? len - 1 : num] = 0;

    return num;
}

void wificfg_form_url_decode(char *string)
{
    char *src = string;
    char *src_end = string + strlen(string);
    char *dst = string;

    while (src < src_end) {
        char c = *src++;
        if (c == '+') {
            c = ' ';
        } else if (c == '%' && src < src_end - 1) {
            unsigned char c1 = src[0];
            unsigned char c2 = src[1];
            if (isxdigit(c1) && isxdigit(c2)) {
                c1 = tolower(c1);
                int d1 = (c1 >= 'a' && c1 <= 'z') ? c1 - 'a' + 10 : c1 - '0';
                c2 = tolower(c2);
                int d2 = (c2 >= 'a' && c2 <= 'z') ? c2 - 'a' + 10 : c2 - '0';
                *dst++ = (d1 << 4) + d2;
                src += 2;
                continue;
            }
        }
        *dst++ = c;
    }

    *dst = 0;
}

/* HTML escaping. */
void wificfg_html_escape(char *string, char *buf, size_t len)
{
    size_t i;
    size_t out = 0;

    for (i = 0, out = 0; out < len - 1; ) {
        char c = string[i++];
        if (!c)
            break;

        if (c == '&') {
            if (out >= len - 5)
                break;
            buf[out] = '&';
            buf[out + 1] = 'a';
            buf[out + 2] = 'm';
            buf[out + 3] = 'p';
            buf[out + 4] = ';';
            out += 5;
            continue;
        }
        if (c == '"') {
            if (out >= len - 6)
                break;
            buf[out] = '&';
            buf[out + 1] = 'q';
            buf[out + 2] = 'u';
            buf[out + 3] = 'o';
            buf[out + 4] = 't';
            buf[out + 5] = ';';
            out += 6;
            continue;
        }
        if (c == '<') {
            if (out >= len - 4)
                break;
            buf[out] = '&';
            buf[out + 1] = 'l';
            buf[out + 2] = 't';
            buf[out + 3] = ';';
            out += 4;
            continue;
        }
        if (c == '>') {
            if (out >= len - 4)
                break;
            buf[out] = '&';
            buf[out + 1] = 'g';
            buf[out + 2] = 't';
            buf[out + 3] = ';';
            out += 4;
            continue;
        }

        buf[out++] = c;
    }

    buf[out] = 0;
}

/* Various keywords are interned as they are read. */

static const struct {
    const char *str;
    wificfg_method method;
} method_table[] = {
    {"get", HTTP_METHOD_GET},
    {"post", HTTP_METHOD_POST},
    {"head", HTTP_METHOD_HEAD}
};

static wificfg_method intern_http_method(char *str)
{
    size_t i;
    for (i = 0;  i < sizeof(method_table) / sizeof(method_table[0]); i++) {
        if (!strcmp(str, method_table[i].str))
            return method_table[i].method;
    }
    return HTTP_METHOD_OTHER;
}

/*
 * The web server recognizes only these header names. Other headers are ignored.
 */
typedef enum {
    HTTP_HEADER_HOST,
    HTTP_HEADER_CONTENT_LENGTH,
    HTTP_HEADER_CONTENT_TYPE,
    HTTP_HEADER_CONNECTION,
    HTTP_HEADER_OTHER
} http_header;

static const struct {
    const char *str;
    http_header name;
} http_header_table[] = {
    {"host", HTTP_HEADER_HOST},
    {"content-length", HTTP_HEADER_CONTENT_LENGTH},
    {"content-type", HTTP_HEADER_CONTENT_TYPE},
    {"connection", HTTP_HEADER_CONNECTION}
};

static http_header intern_http_header(char *str)
{
    size_t i;
    for (i = 0;  i < sizeof(http_header_table) / sizeof(http_header_table[0]); i++) {
        if (!strcmp(str, http_header_table[i].str))
            return http_header_table[i].name;
    }
    return HTTP_HEADER_OTHER;
}


static const struct {
    const char *str;
    wificfg_content_type type;
} content_type_table[] = {
    {"application/x-www-form-urlencoded", HTTP_CONTENT_TYPE_WWW_FORM_URLENCODED}
};

static wificfg_content_type intern_http_content_type(char *str)
{
    size_t i;
    for (i = 0;  i < sizeof(content_type_table) / sizeof(content_type_table[0]); i++) {
        if (!strcmp(str, content_type_table[i].str))
            return content_type_table[i].type;
    }
    return HTTP_CONTENT_TYPE_OTHER;
}

static char *skip_whitespace(char *string)
{
    while (isspace((unsigned char)*string)) string++;
    return string;
}

static char *skip_to_whitespace(char *string)
{
    do {
        unsigned char c = *string;
        if (!c || isspace(c))
            break;
        string++;
    } while (1);

    return string;
}

ssize_t wificfg_write_string(int s, const char *str)
{
    ssize_t res = write(s, str, strlen(str));
    return res;
}

ssize_t wificfg_write_string_chunk(int s, const char *str, char *buf, size_t len)
{
    size_t str_len = strlen(str);

    if (str_len == 0) {
        /* Can not be encoded, would be EOF. */
        return 0;
    }

    if (str_len + 6 < len) {
        /* Can fit the chunk in the buffer. */
        memmove(buf + 4, str, str_len);
        size_t start = 1;
        if (str_len < 10) {
            buf[1] = '0' + str_len;
        } else if (str_len < 16) {
            buf[1] = 'a' + str_len - 10;
        } else {
            uint32_t digit0 = str_len >> 4;
            if (digit0 < 10) {
                buf[0] = '0' + digit0;
            } else {
                buf[0] = 'a' + digit0 - 10;
            }
            uint32_t digit1 = str_len & 0xf;
            if (digit1 < 10) {
                buf[1] = '0' + digit1;
            } else {
                buf[1] = 'a' + digit1 - 10;
            }
            start = 0;
        }
        buf[2] = '\r';
        buf[3] = '\n';
        buf[4 + str_len] = '\r';
        buf[4 + str_len + 1] = '\n';
        return write(s, buf + start, 4 - start + str_len + 2);
    }

    /* Else too big for the buffer. */
    char size_buf[8];
    size_t size_len = snprintf(size_buf, sizeof(size_buf), "%x\r\n", str_len);
    ssize_t res = write(s, size_buf, size_len);
    if (res != size_len) {
        return res;
    }
    res = write(s, str, str_len);
    if (res != str_len) {
        return res;
    }
    return write(s, size_buf + size_len - 2, 2);
}

ssize_t wificfg_write_chunk_end(int s)
{
    return wificfg_write_string(s, "0\r\n\r\n");
}

typedef enum {
    FORM_NAME_CFG_ENABLE,
    FORM_NAME_CFG_PASSWORD,
    FORM_NAME_HOSTNAME,
    FORM_NAME_STA_ENABLE,
    FORM_NAME_STA_DISABLED_RESTARTS,
    FORM_NAME_STA_SSID,
    FORM_NAME_STA_PASSWORD,
    FORM_NAME_STA_DHCP,
    FORM_NAME_STA_IP_ADDR,
    FORM_NAME_STA_NETMASK,
    FORM_NAME_STA_GATEWAY,
    FORM_NAME_STA_MDNS,
    FORM_NAME_AP_ENABLE,
    FORM_NAME_AP_DISABLE_IF_STA,
    FORM_NAME_AP_DISABLED_RESTARTS,
    FORM_NAME_AP_SSID,
    FORM_NAME_AP_PASSWORD,
    FORM_NAME_AP_SSID_HIDDEN,
    FORM_NAME_AP_CHANNEL,
    FORM_NAME_AP_AUTHMODE,
    FORM_NAME_AP_MAX_CONN,
    FORM_NAME_AP_BEACON_INTERVAL,
    FORM_NAME_AP_IP_ADDR,
    FORM_NAME_AP_NETMASK,
    FORM_NAME_AP_DHCP_LEASES,
    FORM_NAME_AP_DNS,
    FORM_NAME_AP_MDNS,
    FORM_NAME_DONE,
    FORM_NAME_NONE
} form_name;

static const struct {
    const char *str;
    form_name name;
} form_name_table[] = {
    {"cfg_enable", FORM_NAME_CFG_ENABLE},
    {"cfg_password", FORM_NAME_CFG_PASSWORD},
    {"hostname", FORM_NAME_HOSTNAME},
    {"sta_enable", FORM_NAME_STA_ENABLE},
    {"sta_disabled_restarts", FORM_NAME_STA_DISABLED_RESTARTS},
    {"sta_ssid", FORM_NAME_STA_SSID},
    {"sta_dhcp", FORM_NAME_STA_DHCP},
    {"sta_password", FORM_NAME_STA_PASSWORD},
    {"sta_ip_addr", FORM_NAME_STA_IP_ADDR},
    {"sta_netmask", FORM_NAME_STA_NETMASK},
    {"sta_gateway", FORM_NAME_STA_GATEWAY},
    {"sta_mdns", FORM_NAME_STA_MDNS},
    {"ap_enable", FORM_NAME_AP_ENABLE},
    {"ap_disable_if_sta", FORM_NAME_AP_DISABLE_IF_STA},
    {"ap_disabled_restarts", FORM_NAME_AP_DISABLED_RESTARTS},
    {"ap_ssid", FORM_NAME_AP_SSID},
    {"ap_password", FORM_NAME_AP_PASSWORD},
    {"ap_ssid_hidden", FORM_NAME_AP_SSID_HIDDEN},
    {"ap_channel", FORM_NAME_AP_CHANNEL},
    {"ap_authmode", FORM_NAME_AP_AUTHMODE},
    {"ap_max_conn", FORM_NAME_AP_MAX_CONN},
    {"ap_beacon_interval", FORM_NAME_AP_BEACON_INTERVAL},
    {"ap_ip_addr", FORM_NAME_AP_IP_ADDR},
    {"ap_netmask", FORM_NAME_AP_NETMASK},
    {"ap_dhcp_leases", FORM_NAME_AP_DHCP_LEASES},
    {"ap_dns", FORM_NAME_AP_DNS},
    {"ap_mdns", FORM_NAME_AP_MDNS},
    {"done", FORM_NAME_DONE}
};

static form_name intern_form_name(char *str)
{
    size_t i;
    for (i = 0;  i < sizeof(form_name_table) / sizeof(form_name_table[0]); i++) {
        if (!strcmp(str, form_name_table[i].str))
            return form_name_table[i].name;
    }
    return FORM_NAME_NONE;
}


static const char *http_favicon[] = {
#include "content/favicon.ico"
};

static int handle_favicon(int s, wificfg_method method,
                          uint32_t content_length,
                          wificfg_content_type content_type,
                          char *buf, size_t len)
{
    if (wificfg_write_string(s, http_favicon[0]) < 0) return -1;

    if (method != HTTP_METHOD_HEAD) {
        if (wificfg_write_string_chunk(s, http_favicon[1], buf, len) < 0) return -1;
        if (wificfg_write_chunk_end(s) < 0) return -1;
    }
    return 0;
}

// .value-lg{font-size:24px}.label-extra{display:block;font-style:italic;font-size:13px}
// devo: "Cache-Control: no-store\r\n"
static const char *http_style[] = {
#include "content/style.css"
};


static int handle_style(int s, wificfg_method method,
                        uint32_t content_length,
                        wificfg_content_type content_type,
                        char *buf, size_t len)
{
    if (wificfg_write_string(s, http_style[0]) < 0) return -1;

    if (method != HTTP_METHOD_HEAD) {
        if (wificfg_write_string_chunk(s, http_style[1], buf, len) < 0) return -1;
        if (wificfg_write_chunk_end(s) < 0) return -1;
    }
    return 0;
}

static const char *http_script[] = {
#include "content/script.js"
};

static int handle_script(int s, wificfg_method method,
                         uint32_t content_length,
                         wificfg_content_type content_type,
                         char *buf, size_t len)
{
    if (wificfg_write_string(s, http_script[0]) < 0) return -1;

    if (method != HTTP_METHOD_HEAD) {
        if (wificfg_write_string_chunk(s, http_script[1], buf, len) < 0) return -1;
        if (wificfg_write_chunk_end(s) < 0) return -1;
    }
    return 0;
}


static const char http_success_header[] = "HTTP/1.1 200 \r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Cache-Control: no-store\r\n"
    "Transfer-Encoding: chunked\r\n"
    "Connection: close\r\n"
    "\r\n";

static const char http_redirect_header[] = "HTTP/1.1 302 \r\n"
    "Location: /wificfg/\r\n"
    "Content-Length: 0\r\n"
    "Connection: close\r\n"
    "\r\n";

static int handle_wificfg_redirect(int s, wificfg_method method,
                                   uint32_t content_length,
                                   wificfg_content_type content_type,
                                   char *buf, size_t len)
{
    return wificfg_write_string(s, http_redirect_header);
}

static int handle_ipaddr_redirect(int s, char *buf, size_t len)
{
    if (wificfg_write_string(s, "HTTP/1.1 302 \r\nLocation: http://") < 0) return -1;

    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);
    if (getsockname(s, (struct sockaddr *)&addr, &addr_len) == 0) {
        if (((struct sockaddr *)&addr)->sa_family == AF_INET) {
            struct sockaddr_in *sa = (struct sockaddr_in *)&addr;
            snprintf(buf, len, IPSTR, IP2STR((ip4_addr_t *)&sa->sin_addr.s_addr));
            if (wificfg_write_string(s, buf) < 0) return -1;
        }
#if LWIP_IPV6
        if (((struct sockaddr *)&addr)->sa_family == AF_INET6) {
            struct sockaddr_in6 *sa = (struct sockaddr_in6 *)&addr;
            const ip6_addr_t *addr6 = (const ip6_addr_t*)&(sa->sin6_addr);
            if (ip6_addr_isipv4mappedipv6(addr6)) {
                snprintf(buf, len, IPSTR, IP2STR((ip4_addr_t *)&addr6->addr[3]));
                if (wificfg_write_string(s, buf) < 0) return -1;
            } else {
                if (wificfg_write_string(s, "[") < 0) return -1;
                if (ip6addr_ntoa_r(addr6, buf, len)) {
                    if (wificfg_write_string(s, buf) < 0) return -1;
                }
                if (wificfg_write_string(s, "]") < 0) return -1;
            }
        }
#endif
    }

    /* Always close here - expect a new connection. */
    return wificfg_write_string(s, "\r\nContent-Length: 0\r\n"
                                "Connection: close\r\n"
                                "\r\n");
}

int wificfg_write_html_title(int s, char *buf, size_t len, const char *str)
{
    /* Use the hostname or AP SSID as the title prefix. */
    char *hostname = NULL;
    sysparam_get_string("hostname", &hostname);
    if (!hostname) {
        sysparam_get_string("wifi_ap_ssid", &hostname);
    }
    if (hostname) {
        wificfg_html_escape(hostname, buf, len);
        free(hostname);
        if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
        if (str) {
            if (wificfg_write_string_chunk(s, " ", buf, len) < 0) return -1;
            if (wificfg_write_string_chunk(s, str, buf, len) < 0) return -1;
        }
    }

    return 0;
}

static const char *http_wificfg_content[] = {
#include "content/wificfg/index.html"
};

static int handle_wificfg_index(int s, wificfg_method method,
                                uint32_t content_length,
                                wificfg_content_type content_type,
                                char *buf, size_t len)
{
    if (wificfg_write_string(s, http_success_header) < 0) return -1;

    if (method != HTTP_METHOD_HEAD) {
        if (wificfg_write_string_chunk(s, http_wificfg_content[0], buf, len) < 0) return -1;
        if (wificfg_write_html_title(s, buf, len, "Wifi Config") < 0) return -1;
        if (wificfg_write_string_chunk(s, http_wificfg_content[1], buf, len) < 0) return -1;

        char *hostname = NULL;
        sysparam_get_string("hostname", &hostname);
        if (hostname) {
            if (wificfg_write_string_chunk(s, "<dt>Hostname</dt><dd>", buf, len) < 0) {
                free(hostname);
                return -1;
            }
            wificfg_html_escape(hostname, buf, len);
            free(hostname);
            if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
            if (wificfg_write_string_chunk(s, "</dd>", buf, len) < 0) return -1;
        }

        uint32_t chip_id = sdk_system_get_chip_id();
        snprintf(buf, len, "<dt>Chip ID</dt><dd>%08x</dd>", chip_id);
        if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;

        snprintf(buf, len, "<dt>Uptime</dt><dd>%u seconds</dd>",
                 xTaskGetTickCount() * portTICK_PERIOD_MS / 1000);
        if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;

        snprintf(buf, len, "<dt>Free heap</dt><dd>%u bytes</dd>", (int)xPortGetFreeHeapSize());
        if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;

        snprintf(buf, len, "<dt>Flash ID</dt><dd>0x%08x</dd>", sdk_spi_flash_get_id());
        if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;

        snprintf(buf, len, "<dt>Flash size</dt><dd>%u KiB</dd>", sdk_flashchip.chip_size >> 10);
        if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;

        if (wificfg_write_string_chunk(s, "<dt>LwIP version</dt><dd>" LWIP_VERSION_STRING "</dd>", buf, len) < 0) return -1;
        if (wificfg_write_string_chunk(s, "<dt>FreeRTOS version</dt><dd>" tskKERNEL_VERSION_NUMBER "</dd>", buf, len) < 0) return -1;
        if (wificfg_write_string_chunk(s, "<dt>Newlib version</dt><dd>" _NEWLIB_VERSION "</dd>", buf, len) < 0) return -1;

        enum sdk_sleep_type sleep_type = sdk_wifi_get_sleep_type();
        const char *sleep_type_str = "??";
        switch (sleep_type) {
        case WIFI_SLEEP_NONE:
            sleep_type_str = "None";
            break;
        case WIFI_SLEEP_LIGHT:
            sleep_type_str = "Light";
            break;
        case WIFI_SLEEP_MODEM:
            sleep_type_str = "Modem";
            break;
        default:
            break;
        }
        snprintf(buf, len, "<dt>WiFi sleep type</dt><dd>%s</dd>", sleep_type_str);
        if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;

        uint8_t opmode = sdk_wifi_get_opmode();
        const char *opmode_str = "??";
        switch (opmode) {
        case NULL_MODE:
            opmode_str = "Null";
            break;
        case STATION_MODE:
            opmode_str = "Station";
            break;
        case SOFTAP_MODE:
            opmode_str = "SoftAP";
            break;
        case STATIONAP_MODE:
            opmode_str = "StationAP";
            break;
        default:
            break;
        }
        snprintf(buf, len, "<dt>OpMode</dt><dd>%s</dd>", opmode_str);
        if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;

        if (opmode > NULL_MODE) {
            snprintf(buf, len, "<dt>WiFi channel</dt><dd>%u</dd>", sdk_wifi_get_channel());
            if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;

            const char *phy_mode_str = "??";
            switch (sdk_wifi_get_phy_mode()) {
            case PHY_MODE_11B:
                phy_mode_str = "11b";
                break;
            case PHY_MODE_11G:
                phy_mode_str = "11g";
                break;
            case PHY_MODE_11N:
                phy_mode_str = "11n";
                break;
            default:
                break;
            }
            snprintf(buf, len, "<dt>WiFi physical mode</dt><dd>%s</dd>", phy_mode_str);
            if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
        }

        if (opmode == STATION_MODE || opmode == STATIONAP_MODE) {
            uint8_t hwaddr[6];
            if (sdk_wifi_get_macaddr(STATION_IF, hwaddr)) {
                if (wificfg_write_string_chunk(s, "<dt>Station MAC address</dt>", buf, len) < 0) return -1;
                snprintf(buf, len, "<dd>" MACSTR "</dd>", MAC2STR(hwaddr));
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
            }
            struct ip_info info;
            if (sdk_wifi_get_ip_info(STATION_IF, &info)) {
                if (wificfg_write_string_chunk(s, "<dt>Station IP address</dt>", buf, len) < 0) return -1;
                snprintf(buf, len, "<dd>" IPSTR "</dd>", IP2STR(&info.ip));
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                if (wificfg_write_string_chunk(s, "<dt>Station netmask</dt>", buf, len) < 0) return -1;
                snprintf(buf, len, "<dd>" IPSTR "</dd>", IP2STR(&info.netmask));
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                if (wificfg_write_string_chunk(s, "<dt>Station gateway</dt>", buf, len) < 0) return -1;
                snprintf(buf, len, "<dd>" IPSTR "</dd>", IP2STR(&info.gw));
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
            }
#if LWIP_IPV6
            struct netif *netif = sdk_system_get_netif(STATION_IF);
            if (netif) {
                for (int i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
                    if (ip6_addr_isvalid(netif_ip6_addr_state(netif, i))) {
                        if (wificfg_write_string_chunk(s, "<dt>Station IPv6</dt><dd>", buf, len) < 0) return -1;
                        const ip6_addr_t *addr6 = netif_ip6_addr(netif, i);
                        ip6addr_ntoa_r(addr6, buf, len);
                        if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                        if (wificfg_write_string_chunk(s, "</dd>", buf, len) < 0) return -1;
                    }
                }
            }
#endif
        }

        if (opmode == SOFTAP_MODE || opmode == STATIONAP_MODE) {
            uint8_t hwaddr[6];
            if (sdk_wifi_get_macaddr(SOFTAP_IF, hwaddr)) {
                if (wificfg_write_string_chunk(s, "<dt>AP MAC address</dt>", buf, len) < 0) return -1;
                snprintf(buf, len, "<dd>" MACSTR "</dd>", MAC2STR(hwaddr));
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
            }
            struct ip_info info;
            if (sdk_wifi_get_ip_info(SOFTAP_IF, &info)) {
                if (wificfg_write_string_chunk(s, "<dt>AP IP address</dt>", buf, len) < 0) return -1;
                snprintf(buf, len, "<dd>" IPSTR "</dd>", IP2STR(&info.ip));
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                if (wificfg_write_string_chunk(s, "<dt>AP netmask</dt>", buf, len) < 0) return -1;
                snprintf(buf, len, "<dd>" IPSTR "</dd>", IP2STR(&info.netmask));
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                if (wificfg_write_string_chunk(s, "<dt>AP gateway</dt>", buf, len) < 0) return -1;
                snprintf(buf, len, "<dd>" IPSTR "</dd>", IP2STR(&info.gw));
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
            }

#if LWIP_IPV6
            struct netif *netif = sdk_system_get_netif(SOFTAP_IF);
            if (netif) {
                for (int i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
                    if (ip6_addr_isvalid(netif_ip6_addr_state(netif, i))) {
                        if (wificfg_write_string_chunk(s, "<dt>AP IPv6</dt><dd>", buf, len) < 0) return -1;
                        const ip6_addr_t *addr6 = netif_ip6_addr(netif, i);
                        ip6addr_ntoa_r(addr6, buf, len);
                        if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                        if (wificfg_write_string_chunk(s, "</dd>", buf, len) < 0) return -1;
                    }
                }
            }
#endif
        }

        struct sockaddr_storage addr;
        socklen_t addr_len = sizeof(addr);
        if (getpeername(s, (struct sockaddr *)&addr, &addr_len) == 0) {
            if (((struct sockaddr *)&addr)->sa_family == AF_INET) {
                struct sockaddr_in *sa = (struct sockaddr_in *)&addr;
                if (wificfg_write_string_chunk(s, "<dt>Peer address</dt>", buf, len) < 0) return -1;
                snprintf(buf, len, "<dd>" IPSTR ", port %u</dd>",
                         IP2STR((ip4_addr_t *)&sa->sin_addr.s_addr), ntohs(sa->sin_port));
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
            }

#if LWIP_IPV6
            if (((struct sockaddr *)&addr)->sa_family == AF_INET6) {
                struct sockaddr_in6 *sa = (struct sockaddr_in6 *)&addr;
                if (wificfg_write_string_chunk(s, "<dt>Peer address</dt><dd>", buf, len) < 0) return -1;
                const ip6_addr_t *addr6 = (const ip6_addr_t*)&(sa->sin6_addr);
                if (ip6_addr_isipv4mappedipv6(addr6)) {
                    snprintf(buf, len, IPSTR, IP2STR((ip4_addr_t *)&addr6->addr[3]));
                    if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                } else {
                    if (ip6addr_ntoa_r(addr6, buf, len)) {
                        if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                    }
                }
                snprintf(buf, len, ", port %u</dd>", ntohs(sa->sin6_port));
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
            }
#endif
        }

        if (wificfg_write_string_chunk(s, http_wificfg_content[2], buf, len) < 0) return -1;

        char *password = NULL;
        sysparam_get_string("cfg_password", &password);
        if (password) {
            wificfg_html_escape(password, buf, len);
            free(password);
            if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
        }

        if (wificfg_write_string_chunk(s, http_wificfg_content[3], buf, len) < 0) return -1;

        if (wificfg_write_chunk_end(s) < 0) return -1;
    }
    return 0;
}

static int handle_wificfg_index_post(int s, wificfg_method method,
                                     uint32_t content_length,
                                     wificfg_content_type content_type,
                                     char *buf, size_t len)
{
    if (content_type != HTTP_CONTENT_TYPE_WWW_FORM_URLENCODED) {
        return wificfg_write_string(s, "HTTP/1.1 400 \r\n"
                                    "Content-Type: text/html\r\n"
                                    "Content-Length: 0\r\n"
                                    "Connection: close\r\n\r\n");
    }

    size_t rem = content_length;
    bool valp = false;

    while (rem > 0) {
        ssize_t r = wificfg_form_name_value(s, &valp, &rem, buf, len);

        if (r < 0) {
            break;
        }

        wificfg_form_url_decode(buf);

        form_name name = intern_form_name(buf);

        if (valp) {
            ssize_t r = wificfg_form_name_value(s, NULL, &rem, buf, len);
            if (r < 0) {
                break;
            }

            wificfg_form_url_decode(buf);

            switch (name) {
            case FORM_NAME_CFG_ENABLE: {
                uint8_t enable = strtoul(buf, NULL, 10) != 0;
                sysparam_set_int8("cfg_enable", enable);
                break;
            }
            case FORM_NAME_CFG_PASSWORD:
                sysparam_set_string("cfg_password", buf);
                break;
            default:
                break;
            }
        }
    }

    return wificfg_write_string(s, http_redirect_header);
}

static const char *http_wifi_station_content[] = {
#include "content/wificfg/sta.html"
};

static int handle_wifi_station(int s, wificfg_method method,
                               uint32_t content_length,
                               wificfg_content_type content_type,
                               char *buf, size_t len)
{
    if (wificfg_write_string(s, http_success_header) < 0) return -1;

    if (method != HTTP_METHOD_HEAD) {
        if (wificfg_write_string_chunk(s, http_wifi_station_content[0], buf, len) < 0) return -1;
        if (wificfg_write_html_title(s, buf, len, "Wifi station") < 0) return -1;
        if (wificfg_write_string_chunk(s, http_wifi_station_content[1], buf, len) < 0) return -1;

        int8_t wifi_sta_enable = 1;
        sysparam_get_int8("wifi_sta_enable", &wifi_sta_enable);
        if (wifi_sta_enable && wificfg_write_string_chunk(s, "checked", buf, len) < 0) return -1;
        if (wificfg_write_string_chunk(s, http_wifi_station_content[2], buf, len) < 0) return -1;

        int8_t wifi_sta_disabled_restarts = 0;
        sysparam_get_int8("wifi_sta_disabled_restarts", &wifi_sta_disabled_restarts);
        snprintf(buf, len, "%u", wifi_sta_disabled_restarts);
        if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
        if (wificfg_write_string_chunk(s, http_wifi_station_content[3], buf, len) < 0) return -1;

        char *wifi_sta_ssid = NULL;
        sysparam_get_string("wifi_sta_ssid", &wifi_sta_ssid);
        if (wifi_sta_ssid) {
            wificfg_html_escape(wifi_sta_ssid, buf, len);
            free(wifi_sta_ssid);
            if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
        }

        if (wificfg_write_string_chunk(s, http_wifi_station_content[4], buf, len) < 0) return -1;

        char *wifi_sta_password = NULL;
        sysparam_get_string("wifi_sta_password", &wifi_sta_password);
        if (wifi_sta_password) {
            wificfg_html_escape(wifi_sta_password, buf, len);
            free(wifi_sta_password);
            if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
        }

        if (wificfg_write_string_chunk(s, http_wifi_station_content[5], buf, len) < 0) return -1;

        char *hostname = NULL;
        sysparam_get_string("hostname", &hostname);
        if (hostname) {
            wificfg_html_escape(hostname, buf, len);
            free(hostname);
            if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
        }

        if (wificfg_write_string_chunk(s, http_wifi_station_content[6], buf, len) < 0) return -1;

        int8_t wifi_sta_dhcp = 1;
        sysparam_get_int8("wifi_sta_dhcp", &wifi_sta_dhcp);
        if (wifi_sta_dhcp && wificfg_write_string_chunk(s, "checked", buf, len) < 0) return -1;
        if (wificfg_write_string_chunk(s, http_wifi_station_content[7], buf, len) < 0) return -1;
        if (!wifi_sta_dhcp && wificfg_write_string_chunk(s, "checked", buf, len) < 0) return -1;

        if (wificfg_write_string_chunk(s, http_wifi_station_content[8], buf, len) < 0) return -1;

        char *wifi_sta_ip_addr = NULL;
        sysparam_get_string("wifi_sta_ip_addr", &wifi_sta_ip_addr);
        if (wifi_sta_ip_addr) {
            wificfg_html_escape(wifi_sta_ip_addr, buf, len);
            free(wifi_sta_ip_addr);
            if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
        }

        if (wificfg_write_string_chunk(s, http_wifi_station_content[9], buf, len) < 0) return -1;

        char *wifi_sta_netmask = NULL;
        sysparam_get_string("wifi_sta_netmask", &wifi_sta_netmask);
        if (wifi_sta_netmask) {
            wificfg_html_escape(wifi_sta_netmask, buf, len);
            free(wifi_sta_netmask);
            if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
        }

        if (wificfg_write_string_chunk(s, http_wifi_station_content[10], buf, len) < 0) return -1;

        char *wifi_sta_gateway = NULL;
        sysparam_get_string("wifi_sta_gateway", &wifi_sta_gateway);
        if (wifi_sta_gateway) {
            wificfg_html_escape(wifi_sta_gateway, buf, len);
            free(wifi_sta_gateway);
            if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
        }

        if (wificfg_write_string_chunk(s, http_wifi_station_content[11], buf, len) < 0) return -1;

        int8_t wifi_sta_mdns = 1;
        sysparam_get_int8("wifi_sta_mdns", &wifi_sta_mdns);
        if (wifi_sta_mdns && wificfg_write_string_chunk(s, "checked", buf, len) < 0) return -1;

        if (wificfg_write_string_chunk(s, http_wifi_station_content[12], buf, len) < 0) return -1;

        if (wificfg_write_chunk_end(s) < 0) return -1;
    }
    return 0;
}

static int handle_wifi_station_post(int s, wificfg_method method,
                                    uint32_t content_length,
                                    wificfg_content_type content_type,
                                    char *buf, size_t len)
{
    if (content_type != HTTP_CONTENT_TYPE_WWW_FORM_URLENCODED) {
        return wificfg_write_string(s, "HTTP/1.1 400 \r\n"
                                    "Content-Type: text/html\r\n"
                                    "Content-Length: 0\r\n"
                                    "Connection: close\r\n\r\n");
    }

    size_t rem = content_length;
    bool valp = false;

    /* Delay committing some values until all have been read. */
    bool done = false;
    uint8_t sta_enable = 0;
    uint8_t mdns_enable = 0;

    while (rem > 0) {
        int r = wificfg_form_name_value(s, &valp, &rem, buf, len);

        if (r < 0) {
            break;
        }

        wificfg_form_url_decode(buf);

        form_name name = intern_form_name(buf);

        if (valp) {
            int r = wificfg_form_name_value(s, NULL, &rem, buf, len);
            if (r < 0) {
                break;
            }

            wificfg_form_url_decode(buf);

            switch (name) {
            case FORM_NAME_STA_ENABLE: {
                sta_enable = strtoul(buf, NULL, 10) != 0;
                break;
            }
            case FORM_NAME_STA_DISABLED_RESTARTS: {
                uint32_t restarts = strtoul(buf, NULL, 10);
                if (restarts <= 255)
                    sysparam_set_int8("wifi_sta_disabled_restarts", restarts);
                break;
            }
            case FORM_NAME_STA_SSID:
                sysparam_set_string("wifi_sta_ssid", buf);
                break;
            case FORM_NAME_STA_PASSWORD:
                sysparam_set_string("wifi_sta_password", buf);
                break;
            case FORM_NAME_HOSTNAME:
                sysparam_set_string("hostname", buf);
                break;
            case FORM_NAME_STA_DHCP: {
                uint8_t enable = strtoul(buf, NULL, 10) != 0;
                sysparam_set_int8("wifi_sta_dhcp", enable);
                break;
            }
            case FORM_NAME_STA_IP_ADDR:
                sysparam_set_string("wifi_sta_ip_addr", buf);
                break;
            case FORM_NAME_STA_NETMASK:
                sysparam_set_string("wifi_sta_netmask", buf);
                break;
            case FORM_NAME_STA_GATEWAY:
                sysparam_set_string("wifi_sta_gateway", buf);
                break;
            case FORM_NAME_STA_MDNS: {
                mdns_enable = strtoul(buf, NULL, 10) != 0;
                break;
            }
            case FORM_NAME_DONE:
                done = true;
                break;
            default:
                break;
            }
        }
    }

    if (done) {
        sysparam_set_int8("wifi_sta_enable", sta_enable);
        sysparam_set_int8("wifi_sta_mdns", mdns_enable);
    }

    return wificfg_write_string(s, http_redirect_header);
}

static const char *http_wifi_ap_content[] = {
#include "content/wificfg/ap.html"
};

static int handle_wifi_ap(int s, wificfg_method method,
                          uint32_t content_length,
                          wificfg_content_type content_type,
                          char *buf, size_t len)
{
    if (wificfg_write_string(s, http_success_header) < 0) return -1;

    if (method != HTTP_METHOD_HEAD) {
        if (wificfg_write_string_chunk(s, http_wifi_ap_content[0], buf, len) < 0) return -1;
        if (wificfg_write_html_title(s, buf, len, "Wifi access point") < 0) return -1;
        if (wificfg_write_string_chunk(s, http_wifi_ap_content[1], buf, len) < 0) return -1;

        int8_t wifi_ap_enable = 1;
        sysparam_get_int8("wifi_ap_enable", &wifi_ap_enable);
        if (wifi_ap_enable && wificfg_write_string_chunk(s, "checked", buf, len) < 0) return -1;

        if (wificfg_write_string_chunk(s, http_wifi_ap_content[2], buf, len) < 0) return -1;

        int8_t wifi_ap_disable_if_sta = 1;
        sysparam_get_int8("wifi_ap_disable_if_sta", &wifi_ap_disable_if_sta);
        if (wifi_ap_disable_if_sta && wificfg_write_string_chunk(s, "checked", buf, len) < 0) return -1;

        if (wificfg_write_string_chunk(s, http_wifi_ap_content[3], buf, len) < 0) return -1;

        int8_t wifi_ap_disabled_restarts = 0;
        sysparam_get_int8("wifi_ap_disabled_restarts", &wifi_ap_disabled_restarts);
        snprintf(buf, len, "%u", wifi_ap_disabled_restarts);
        if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;

        if (wificfg_write_string_chunk(s, http_wifi_ap_content[4], buf, len) < 0) return -1;

        char *wifi_ap_ssid = NULL;
        sysparam_get_string("wifi_ap_ssid", &wifi_ap_ssid);
        if (wifi_ap_ssid) {
            wificfg_html_escape(wifi_ap_ssid, buf, len);
            free(wifi_ap_ssid);
            if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
        }

        if (wificfg_write_string_chunk(s, http_wifi_ap_content[5], buf, len) < 0) return -1;

        char *wifi_ap_password = NULL;
        sysparam_get_string("wifi_ap_password", &wifi_ap_password);
        if (wifi_ap_password) {
            wificfg_html_escape(wifi_ap_password, buf, len);
            free(wifi_ap_password);
            if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
        }

        if (wificfg_write_string_chunk(s, http_wifi_ap_content[6], buf, len) < 0) return -1;

        int8_t wifi_ap_ssid_hidden = 0;
        sysparam_get_int8("wifi_ap_ssid_hidden", &wifi_ap_ssid_hidden);
        if (wifi_ap_ssid_hidden && wificfg_write_string_chunk(s, "checked", buf, len) < 0) return -1;

        if (wificfg_write_string_chunk(s, http_wifi_ap_content[7], buf, len) < 0) return -1;

        int8_t wifi_ap_channel = 6;
        sysparam_get_int8("wifi_ap_channel", &wifi_ap_channel);
        snprintf(buf, len, "%u", wifi_ap_channel);
        if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;

        if (wificfg_write_string_chunk(s, http_wifi_ap_content[8], buf, len) < 0) return -1;

        int8_t wifi_ap_authmode = AUTH_WPA_WPA2_PSK;
        sysparam_get_int8("wifi_ap_authmode", &wifi_ap_authmode);
        if (wifi_ap_authmode == AUTH_OPEN && wificfg_write_string_chunk(s, " selected", buf, len) < 0) return -1;
        if (wificfg_write_string_chunk(s, http_wifi_ap_content[9], buf, len) < 0) return -1;
        if (wifi_ap_authmode == AUTH_WPA_PSK && wificfg_write_string_chunk(s, " selected", buf, len) < 0) return -1;
        if (wificfg_write_string_chunk(s, http_wifi_ap_content[10], buf, len) < 0) return -1;
        if (wifi_ap_authmode == AUTH_WPA2_PSK && wificfg_write_string_chunk(s, " selected", buf, len) < 0) return -1;
        if (wificfg_write_string_chunk(s, http_wifi_ap_content[11], buf, len) < 0) return -1;
        if (wifi_ap_authmode == AUTH_WPA_WPA2_PSK && wificfg_write_string_chunk(s, " selected", buf, len) < 0) return -1;

        if (wificfg_write_string_chunk(s, http_wifi_ap_content[12], buf, len) < 0) return -1;

        int8_t wifi_ap_max_conn = 3;
        sysparam_get_int8("wifi_ap_max_conn", &wifi_ap_max_conn);
        snprintf(buf, len, "%u", wifi_ap_max_conn);
        if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;

        if (wificfg_write_string_chunk(s, http_wifi_ap_content[13], buf, len) < 0) return -1;

        int32_t wifi_ap_beacon_interval = 100;
        sysparam_get_int32("wifi_ap_beacon_interval", &wifi_ap_beacon_interval);
        snprintf(buf, len, "%u", wifi_ap_beacon_interval);
        if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;

        if (wificfg_write_string_chunk(s, http_wifi_ap_content[14], buf, len) < 0) return -1;

        char *wifi_ap_ip_addr = NULL;
        sysparam_get_string("wifi_ap_ip_addr", &wifi_ap_ip_addr);
        if (wifi_ap_ip_addr) {
            wificfg_html_escape(wifi_ap_ip_addr, buf, len);
            free(wifi_ap_ip_addr);
            if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
        }

        if (wificfg_write_string_chunk(s, http_wifi_ap_content[15], buf, len) < 0) return -1;

        char *wifi_ap_netmask = NULL;
        sysparam_get_string("wifi_ap_netmask", &wifi_ap_netmask);
        if (wifi_ap_netmask) {
            wificfg_html_escape(wifi_ap_netmask, buf, len);
            free(wifi_ap_netmask);
            if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
        }

        if (wificfg_write_string_chunk(s, http_wifi_ap_content[16], buf, len) < 0) return -1;

        int8_t wifi_ap_dhcp_leases = 4;
        sysparam_get_int8("wifi_ap_dhcp_leases", &wifi_ap_dhcp_leases);
        snprintf(buf, len, "%u", wifi_ap_dhcp_leases);
        if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;

        if (wificfg_write_string_chunk(s, http_wifi_ap_content[17], buf, len) < 0) return -1;

        int8_t wifi_ap_dns = 1;
        sysparam_get_int8("wifi_ap_dns", &wifi_ap_dns);
        if (wifi_ap_dns && wificfg_write_string_chunk(s, "checked", buf, len) < 0) return -1;

        if (wificfg_write_string_chunk(s, http_wifi_ap_content[18], buf, len) < 0) return -1;

        int8_t wifi_ap_mdns = 1;
        sysparam_get_int8("wifi_ap_mdns", &wifi_ap_mdns);
        if (wifi_ap_mdns && wificfg_write_string_chunk(s, "checked", buf, len) < 0) return -1;

        if (wificfg_write_string_chunk(s, http_wifi_ap_content[19], buf, len) < 0) return -1;

        if (wificfg_write_chunk_end(s) < 0) return -1;
    }
    return 0;
}

static int handle_wifi_ap_post(int s, wificfg_method method,
                               uint32_t content_length,
                               wificfg_content_type content_type,
                               char *buf, size_t len)
{
    if (content_type != HTTP_CONTENT_TYPE_WWW_FORM_URLENCODED) {
        return wificfg_write_string(s, "HTTP/1.1 400 \r\n"
                                    "Content-Type: text/html\r\n"
                                    "Content-Length: 0\r\n"
                                    "Connection: close\r\n\r\n");
    }

    size_t rem = content_length;
    bool valp = false;

    /* Delay committing some values until all have been read. */
    bool done = false;
    uint8_t ap_enable = 0;
    uint8_t ap_disable_if_sta = 0;
    uint8_t ssid_hidden = 0;
    uint8_t dns_enable = 0;
    uint8_t mdns_enable = 0;

    while (rem > 0) {
        ssize_t r = wificfg_form_name_value(s, &valp, &rem, buf, len);

        if (r < 0) {
            break;
        }

        wificfg_form_url_decode(buf);

        form_name name = intern_form_name(buf);

        if (valp) {
            ssize_t r = wificfg_form_name_value(s, NULL, &rem, buf, len);
            if (r < 0) {
                break;
            }

            wificfg_form_url_decode(buf);

            switch (name) {
            case FORM_NAME_AP_ENABLE: {
                ap_enable = strtoul(buf, NULL, 10) != 0;
                break;
            }
            case FORM_NAME_AP_DISABLE_IF_STA: {
                ap_disable_if_sta = strtoul(buf, NULL, 10) != 0;
                break;
            }
            case FORM_NAME_AP_DISABLED_RESTARTS: {
                uint32_t restarts = strtoul(buf, NULL, 10);
                if (restarts <= 255)
                    sysparam_set_int8("wifi_ap_disabled_restarts", restarts);
                break;
            }
            case FORM_NAME_AP_SSID:
                sysparam_set_string("wifi_ap_ssid", buf);
                break;
            case FORM_NAME_AP_PASSWORD:
                sysparam_set_string("wifi_ap_password", buf);
                break;
            case FORM_NAME_AP_SSID_HIDDEN: {
                ssid_hidden = strtoul(buf, NULL, 10) != 0;
                break;
            }
            case FORM_NAME_AP_CHANNEL: {
                uint32_t channel = strtoul(buf, NULL, 10);
                if (channel >= 1 && channel <= 14)
                    sysparam_set_int8("wifi_ap_channel", channel);
                break;
            }
            case FORM_NAME_AP_AUTHMODE: {
                uint32_t mode = strtoul(buf, NULL, 10);
                if (mode == AUTH_OPEN || mode == AUTH_WPA_PSK ||
                    mode == AUTH_WPA2_PSK || mode == AUTH_WPA_WPA2_PSK) {
                    sysparam_set_int8("wifi_ap_authmode", mode);
                }
                break;
            }
            case FORM_NAME_AP_MAX_CONN: {
                uint32_t max_conn = strtoul(buf, NULL, 10);
                if (max_conn <= 8)
                    sysparam_set_int8("wifi_ap_max_conn", max_conn);
                break;
            }
            case FORM_NAME_AP_BEACON_INTERVAL: {
                uint32_t interval = strtoul(buf, NULL, 10);
                if (interval <= 10000)
                    sysparam_set_int32("wifi_ap_beacon_interval", interval);
                break;
            }
            case FORM_NAME_AP_IP_ADDR:
                sysparam_set_string("wifi_ap_ip_addr", buf);
                break;
            case FORM_NAME_AP_NETMASK:
                sysparam_set_string("wifi_ap_netmask", buf);
                break;
            case FORM_NAME_AP_DHCP_LEASES: {
                uint32_t leases = strtoul(buf, NULL, 10);
                if (leases <= 16)
                    sysparam_set_int8("wifi_ap_dhcp_leases", leases);
                break;
            }
            case FORM_NAME_AP_DNS: {
                dns_enable = strtoul(buf, NULL, 10) != 0;
                break;
            }
            case FORM_NAME_AP_MDNS: {
                mdns_enable = strtoul(buf, NULL, 10) != 0;
                break;
            }
            case FORM_NAME_DONE:
                done = true;
                break;
            default:
                break;
            }
        }
    }

    if (done) {
        sysparam_set_int8("wifi_ap_enable", ap_enable);
        sysparam_set_int8("wifi_ap_disable_if_sta", ap_disable_if_sta);
        sysparam_set_int8("wifi_ap_ssid_hidden", ssid_hidden);
        sysparam_set_int8("wifi_ap_dns", dns_enable);
        sysparam_set_int8("wifi_ap_mdns", mdns_enable);
    }

    return wificfg_write_string(s, http_redirect_header);
}

static bool got_sta_connect = false;
void wificfg_got_sta_connect()
{
    /* Only process this once, to not continue adjusting the settings. */
    if (got_sta_connect) {
        return;
    }
    got_sta_connect = true;

    /* Skip if AP not even enabled. */
    int8_t wifi_ap_enable = 1;
    sysparam_get_int8("wifi_ap_enable", &wifi_ap_enable);
    if (!wifi_ap_enable) {
        return;
    }

    int8_t wifi_ap_disable_if_sta = 1;
    sysparam_get_int8("wifi_ap_disable_if_sta", &wifi_ap_disable_if_sta);

    if (wifi_ap_disable_if_sta) {
        int8_t wifi_ap_disabled_restarts = 0;
        sysparam_get_int8("wifi_ap_disabled_restarts", &wifi_ap_disabled_restarts);
        if (wifi_ap_disabled_restarts == 0) {
            sysparam_set_int8("wifi_ap_disabled_restarts", 1);
        }
    }
}

void wificfg_wait_until_sta_connected()
{
    while (1) {
        uint8_t connect_status = sdk_wifi_station_get_connect_status();
        if (connect_status == STATION_GOT_IP)
            break;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    /*
     * Notifty wificfg to disable the AP interface on the next restart
     * if that option is enabled.
     */
    wificfg_got_sta_connect();
}

struct shutdown_hook {
  struct shutdown_hook *next;
  void (*fn)(void *);
  void *arg;
};

static struct shutdown_hook *shutdown_hooks;

bool wificfg_add_shutdown_hook(void (*fn)(void *), void *arg)
{
  struct shutdown_hook *hook = malloc(sizeof(struct shutdown_hook));

  if (!hook) {
    return false;
  }

  hook->next = shutdown_hooks;
  hook->fn = fn;
  hook->arg = arg;
  shutdown_hooks = hook;
  return true;
}

static int handle_restart_post(int s, wificfg_method method,
                               uint32_t content_length,
                               wificfg_content_type content_type,
                               char *buf, size_t len)
{
    wificfg_write_string(s, http_redirect_header);
    close(s);
    struct shutdown_hook *hook;
    for (hook = shutdown_hooks; hook; hook = hook->next) {
      hook->fn(hook->arg);
    }
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    sdk_system_restart();
    return 0;
}

static int handle_erase_post(int s, wificfg_method method,
                             uint32_t content_length,
                             wificfg_content_type content_type,
                             char *buf, size_t len)
{
    wificfg_write_string(s, http_redirect_header);
    close(s);
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    /*
     * Erase the area starting from the sysparams to the end of the flash.
     * Configuration information may be in the sdk parameter area too, which is
     * in these sectors.
     */
    uint32_t num_sectors = 5 + DEFAULT_SYSPARAM_SECTORS;
    uint32_t start = sdk_flashchip.chip_size - num_sectors * sdk_flashchip.sector_size;
    uint32_t i;
    vPortEnterCritical();
    for (i = 0; i < num_sectors; i++) {
        spiflash_erase_sector(start + i * sdk_flashchip.sector_size);
    }
    sdk_system_restart();
    return 0;
}

/* Minimal not-found response. */
static const char not_found_header[] = "HTTP/1.1 404 \r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Cache-Control: no-store\r\n"
    "Content-Length: 0\r\n"
    "Connection: close\r\n"
    "\r\n";

static const char *http_wificfg_challenge_content[] = {
#include "content/challenge.html"
};

static int handle_wificfg_challenge(int s, wificfg_method method,
                                    uint32_t content_length,
                                    wificfg_content_type content_type,
                                    char *buf, size_t len)
{
    if (wificfg_write_string(s, http_success_header) < 0) return -1;

    if (method != HTTP_METHOD_HEAD) {
        if (wificfg_write_string_chunk(s, http_wificfg_challenge_content[0], buf, len) < 0) return -1;
        if (wificfg_write_html_title(s, buf, len, "Challenge") < 0) return -1;
        if (wificfg_write_string_chunk(s, http_wificfg_challenge_content[1], buf, len) < 0) return -1;
        if (wificfg_write_chunk_end(s) < 0) return -1;
    }
    return 0;
}

static int handle_wificfg_challenge_post(int s, wificfg_method method,
                                         uint32_t content_length,
                                         wificfg_content_type content_type,
                                         char *buf, size_t len)
{
    if (content_type != HTTP_CONTENT_TYPE_WWW_FORM_URLENCODED) {
        return wificfg_write_string(s, "HTTP/1.1 400 \r\n"
                                    "Content-Type: text/html\r\n"
                                    "Content-Length: 0\r\n"
                                    "Connection: close\r\n\r\n");
    }
    size_t rem = content_length;
    bool valp = false;

    int8_t enable = 1;
    sysparam_get_int8("cfg_enable", &enable);
    char *password = NULL;
    sysparam_get_string("cfg_password", &password);

    if (!enable && password && strlen(password)) {
        while (rem > 0) {
            int r = wificfg_form_name_value(s, &valp, &rem, buf, len);

            if (r < 0) {
                break;
            }

            wificfg_form_url_decode(buf);

            form_name name = intern_form_name(buf);

            if (valp) {
                int r = wificfg_form_name_value(s, NULL, &rem, buf, len);
                if (r < 0) {
                    break;
                }

                wificfg_form_url_decode(buf);

                switch (name) {
                case FORM_NAME_CFG_PASSWORD:
                    if (strcmp(password, buf) == 0)
                        sysparam_set_int8("cfg_enable", 1);
                    break;
                default:
                    break;
                }
            }
        }
    }

    if (password)
        free(password);

    return wificfg_write_string(s, http_redirect_header);
}

#ifdef configUSE_TRACE_FACILITY
static const char *http_tasks_content[] = {
#include "content/tasks.html"
};

static int handle_tasks(int s, wificfg_method method,
                        uint32_t content_length,
                        wificfg_content_type content_type,
                        char *buf, size_t len)
{
    if (wificfg_write_string(s, http_success_header) < 0) return -1;

    if (method != HTTP_METHOD_HEAD) {
        if (wificfg_write_string_chunk(s, http_tasks_content[0], buf, len) < 0) return -1;
        if (wificfg_write_html_title(s, buf, len, "Tasks") < 0) return -1;
        if (wificfg_write_string_chunk(s, http_tasks_content[1], buf, len) < 0) return -1;
        int num_tasks = uxTaskGetNumberOfTasks();
        TaskStatus_t *task_status = pvPortMalloc(num_tasks * sizeof(TaskStatus_t));

        if (task_status != NULL) {
            int i;

            if (wificfg_write_string_chunk(s, "<table><tr><th>Task name</th><th>Task number</th><th>Status</th><th>Priority</th><th>Base priority</th><th>Runtime</th><th>Stack high-water</th></tr>", buf, len) < 0) {
                free(task_status);
                return -1;
            }

            /* Generate the (binary) data. */
            num_tasks = uxTaskGetSystemState(task_status, num_tasks, NULL);

            /* Create a human readable table from the binary data. */
            for(i = 0; i < num_tasks; i++) {
                char cStatus;
                switch(task_status[i].eCurrentState) {
                case eRunning: cStatus = '*'; break;
                case eReady: cStatus = 'R'; break;
                case eBlocked: cStatus = 'B'; break;
                case eSuspended: cStatus = 'S'; break;
                case eDeleted: cStatus = 'D'; break;
                default: cStatus = '?'; break;
                }

                snprintf(buf, len, "<tr><th>%s</th>", task_status[i].pcTaskName);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) {
                    free(task_status);
                    return -1;
                }
                snprintf(buf, len, "<td>%u</td><td>%c</td>",
                         (unsigned int)task_status[i].xTaskNumber,
                         cStatus);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) {
                    free(task_status);
                    return -1;
                }
                snprintf(buf, len, "<td>%u</td><td>%u</td>",
                         (unsigned int)task_status[i].uxCurrentPriority,
                         (unsigned int)task_status[i].uxBasePriority);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) {
                    free(task_status);
                    return -1;
                }
                snprintf(buf, len, "<td>%u</td><td>%u</td></tr>",
                         (unsigned int)task_status[i].ulRunTimeCounter,
                         (unsigned int)task_status[i].usStackHighWaterMark);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) {
                    free(task_status);
                    return -1;
                }
            }

            free(task_status);

            if (wificfg_write_string_chunk(s, "</table>", buf, len) < 0) return -1;
        }

        if (wificfg_write_string_chunk(s, http_tasks_content[2], buf, len) < 0) return -1;
        if (wificfg_write_chunk_end(s) < 0) return -1;
    }
    return 0;
}
#endif /* configUSE_TRACE_FACILITY */

static const wificfg_dispatch wificfg_dispatch_list[] = {
    {"/favicon.ico", HTTP_METHOD_GET, handle_favicon, false},
    {"/style.css", HTTP_METHOD_GET, handle_style, false},
    {"/script.js", HTTP_METHOD_GET, handle_script, false},
    {"/", HTTP_METHOD_GET, handle_wificfg_redirect, false},
    {"/index.html", HTTP_METHOD_GET, handle_wificfg_redirect, false},
    {"/wificfg/", HTTP_METHOD_GET, handle_wificfg_index, true},
    {"/wificfg/", HTTP_METHOD_POST, handle_wificfg_index_post, true},
    {"/wificfg/sta.html", HTTP_METHOD_GET, handle_wifi_station, true},
    {"/wificfg/sta.html", HTTP_METHOD_POST, handle_wifi_station_post, true},
    {"/wificfg/ap.html", HTTP_METHOD_GET, handle_wifi_ap, true},
    {"/wificfg/ap.html", HTTP_METHOD_POST, handle_wifi_ap_post, true},
    {"/challenge.html", HTTP_METHOD_GET, handle_wificfg_challenge, false},
    {"/challenge.html", HTTP_METHOD_POST, handle_wificfg_challenge_post, false},
    {"/wificfg/restart.html", HTTP_METHOD_POST, handle_restart_post, true},
    {"/wificfg/erase.html", HTTP_METHOD_POST, handle_erase_post, true},
#ifdef configUSE_TRACE_FACILITY
    {"/tasks", HTTP_METHOD_GET, handle_tasks, false},
    {"/tasks.html", HTTP_METHOD_GET, handle_tasks, false},
#endif /* configUSE_TRACE_FACILITY */
    {NULL, HTTP_METHOD_ANY, NULL}
};

static const wificfg_dispatch wificfg_challenge_dispatch = {"/challenge.html", HTTP_METHOD_GET, handle_wificfg_challenge, false};

typedef struct {
    int32_t port;
    /*
     * Two dispatch lists. First is used for the config pages. Second
     * can be used to extend the pages handled in app code.
     */
    const wificfg_dispatch *wificfg_dispatch;
    const wificfg_dispatch *dispatch;
} server_params;

/*
 * Test if the http host string is a literal IP address. This is needed for the
 * softap mode captive portal and must return false for typical server strings
 * which will be redirected, and true for literal IP address to avoid a redirect
 * for them.
 */
static bool host_is_name(const char *host)
{
    if (host == NULL) {
        return true;
    }

    size_t len = strlen(host);

    if (len < 4) {
        return true;
    }

    char first = host[0];
    char last = host[len - 1];

    if (first == '[' && last == ']') {
        /* Likely an IPv6 address */
        return false;
    }

    if (first >= '0' && first <= '9' && last >= '0' && last <= '9') {
        /* Likely IPv4 address */
        return false;
    }

    return true;
}


/*
 * The http server uses a single thread to service all requests, one request at
 * a time, to keep peak resource usage to a minimum. Keeping connections open
 * would cause delays switching between connections. Thus it closes the
 * connection after each response.
 *
 * To help avoid the resource usage of connections in the time-wait state, the
 * server asks the client to initiate the connection close and waits a short
 * period for it to do so before closing the connection itself.
 *
 * The response length is always well defined, either sending the content-length
 * header or using chunk transfer encoding. Thus the client knows the end of
 * responses without the server having to close the connection, and this allows
 * the client to close the connection.
 *
 * Always closing the connection also allows the connection-close header to be
 * statically bundled in with the response.
 */
static void server_task(void *pvParameters)
{
    char *hostname_local = NULL;
    char *hostname = NULL;

    sysparam_get_string("hostname", &hostname);
    if (hostname) {
        size_t len = strlen(hostname) + 6 + 1;
        hostname_local = (char *)malloc(len);
        if (hostname_local) {
            snprintf(hostname_local, len, "%s.local", hostname);
        }

        int8_t wifi_sta_mdns = 1;
        int8_t wifi_ap_mdns = 1;
        sysparam_get_int8("wifi_sta_mdns", &wifi_sta_mdns);
        sysparam_get_int8("wifi_ap_mdns", &wifi_ap_mdns);

        struct netif *station_netif = sdk_system_get_netif(STATION_IF);
        struct netif *softap_netif = sdk_system_get_netif(SOFTAP_IF);
        if ((wifi_sta_mdns && station_netif) || (wifi_ap_mdns && softap_netif)) {
#if LWIP_MDNS_RESPONDER
            LOCK_TCPIP_CORE();
            mdns_resp_init();
            UNLOCK_TCPIP_CORE();
#endif
#if EXTRAS_MDNS_RESPONDER
            mdns_init();
            mdns_add_facility(hostname, "_http", NULL, mdns_TCP + mdns_Browsable, 80, 600);
#endif
        }
#if LWIP_MDNS_RESPONDER
        LOCK_TCPIP_CORE();
        if (wifi_sta_mdns && station_netif) {
            LOCK_TCPIP_CORE();
            mdns_resp_add_netif(station_netif, hostname, 120);
            mdns_resp_add_service(station_netif, hostname, "_http",
                                  DNSSD_PROTO_TCP, 80, 3600, NULL, NULL);
            UNLOCK_TCPIP_CORE();
        }
        if (wifi_ap_mdns && softap_netif) {
            LOCK_TCPIP_CORE();
            mdns_resp_add_netif(softap_netif, hostname, 120);
            mdns_resp_add_service(softap_netif, hostname, "_http",
                                  DNSSD_PROTO_TCP, 80, 3600, NULL, NULL);
            UNLOCK_TCPIP_CORE();
        }
        UNLOCK_TCPIP_CORE();
#endif

        free(hostname);
    }

    server_params *params = pvParameters;

#if LWIP_IPV6
    int listenfd = socket(AF_INET6, SOCK_STREAM, 0);
    struct sockaddr_in6 serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin6_family = AF_INET6;
    serv_addr.sin6_port = htons(params->port);
    serv_addr.sin6_flowinfo = 0;
    serv_addr.sin6_addr = in6addr_any;
    serv_addr.sin6_scope_id = IP6_NO_ZONE;
#else
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(params->port);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
#endif
    bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    listen(listenfd, 2);

    for (;;) {
        int s = accept(listenfd, (struct sockaddr *)NULL, (socklen_t *)NULL);
        if (s >= 0) {
            const struct timeval timeout = { 10, 0 }; /* 10 second timeout */
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

            /* Buffer for reading the request and headers and the post method
             * names and values. Also used for building dynamically generated
             * responses. */
            char buf[HTTP_BUFFER_SIZE];

            for (;;) {
                /* Read the request line */
                int request_line_size = read_crlf_line(s, buf, sizeof(buf));
                if (request_line_size < 5) {
                    break;
                }

                /* Parse the http method, path, and protocol version. */
                char *method_end = skip_to_whitespace(buf);
                char *path_string = skip_whitespace(method_end);
                *method_end = 0;
                wificfg_method method = intern_http_method(buf);
                char *path_end = skip_to_whitespace(path_string);
                *path_end = 0;

                /* Dispatch to separate functions to handle the requests. */
                const wificfg_dispatch *match = NULL;
                const wificfg_dispatch *dispatch;

                /*
                 * Check the optional application supplied dispatch table
                 * first to allow overriding the wifi config paths.
                 */
                if (params->dispatch) {
                    for (dispatch = params->dispatch; dispatch->path != NULL; dispatch++) {
                        if (strcmp(path_string, dispatch->path) == 0 &&
                            (dispatch->method == HTTP_METHOD_ANY ||
                             method == dispatch->method)) {
                            match = dispatch;
                            break;
                        }
                    }
                }

                if (!match) {
                    for (dispatch = params->wificfg_dispatch; dispatch->path != NULL; dispatch++) {
                        if (strcmp(path_string, dispatch->path) == 0 &&
                            (dispatch->method == HTTP_METHOD_ANY ||
                             method == dispatch->method)) {
                            match = dispatch;
                            break;
                        }
                    }
                }

                if (match && match->secure) {
                    /* A secure url so check if enabled. */
                    int8_t enable = 1;
                    sysparam_get_int8("cfg_enable", &enable);
                    if (!enable) {
                        /* Is there a recovery password? */
                        char *password = NULL;
                        sysparam_get_string("cfg_password", &password);
                        if (password && strlen(password) > 0) {
                            match = &wificfg_challenge_dispatch;
                        } else {
                            match = NULL;
                        }
                        if (password)
                            free(password);
                    }
                }

                /* Read the headers, noting some of interest. */
                wificfg_content_type content_type = HTTP_CONTENT_TYPE_OTHER;
                bool connection_close = false;
                bool host_redirect = false;
                long content_length = 0;

                for (;;) {
                    int header_length = read_crlf_line(s, buf, sizeof(buf));
                    if (header_length <= 0)
                        break;

                    char *name_end = buf;
                    for (; ; name_end++) {
                        char c = *name_end;
                        if (!c || c == ':')
                            break;
                    }
                    if (*name_end == ':') {
                        char *value = name_end + 1;
                        *name_end = 0;
                        http_header header = intern_http_header(buf);
                        value = skip_whitespace(value);
                        switch (header) {
                        case HTTP_HEADER_HOST:
                            if (!host_is_name(value)) {
                                break;
                            }
                            if (!hostname_local ||
                                (hostname_local && strcmp(value, hostname_local))) {
                                host_redirect = true;
                            }
                            break;
                        case HTTP_HEADER_CONTENT_LENGTH:
                            content_length = strtoul(value, NULL, 10);
                            break;
                        case HTTP_HEADER_CONTENT_TYPE:
                            content_type = intern_http_content_type(value);
                            break;
                        case HTTP_HEADER_CONNECTION:
                            connection_close = strcmp(value, "close") == 0;
                            break;
                        default:
                            break;
                        }
                    }
                }

                if (host_redirect) {
                    /* Redirect to an IP address. */
                    handle_ipaddr_redirect(s, buf, sizeof(buf));
                    /* Close the connection. */
                    break;
                }

                if (match) {
                    if ((*match->handler)(s, method, content_length, content_type, buf, sizeof(buf)) < 0) break;
                } else {
                    if (wificfg_write_string(s, not_found_header) < 0) break;
                }

                /*
                 * At this point the client is expected to close the connection,
                 * so wait briefly for it to do so before giving up. While here
                 * consume any excess input to avoid a connection reset - this
                 * can happen if the handler aborted early.
                 */
                const struct timeval timeout1 = { 1, 0 }; /* 1 second timeout */
                setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout1, sizeof(timeout1));
                size_t len;
                for (len = 0; len < 4096; len++) {
                    char c;
                    ssize_t res = read(s, &c, 1);
                    if (res != 1) break;
                }

                if (connection_close)
                    break;

                /* Close anyway. */
                break;
            }

            close(s);

            if (sdk_wifi_station_get_connect_status() == STATION_GOT_IP) {
                wificfg_got_sta_connect();
            }
        }
    }
}


static void dns_task(void *pvParameters)
{
    char *wifi_ap_ip_addr = NULL;
    sysparam_get_string("wifi_ap_ip_addr", &wifi_ap_ip_addr);
    if (!wifi_ap_ip_addr) {
        printf("dns: no ip address\n");
        vTaskDelete(NULL);
    }
    ip4_addr_t server_addr;
    server_addr.addr = ipaddr_addr(wifi_ap_ip_addr);

#if LWIP_IPV6
    int fd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in6 serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin6_family = AF_INET6;
    serv_addr.sin6_port = htons(53);
    serv_addr.sin6_flowinfo = 0;
    serv_addr.sin6_addr = in6addr_any;
    serv_addr.sin6_scope_id = IP6_NO_ZONE;
#else
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(53);
#endif
    bind(fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

    const struct ifreq ifreq0 = { "en0" };
    const struct ifreq ifreq1 = { "en1" };
    setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE,
               sdk_wifi_get_opmode() == STATIONAP_MODE ? &ifreq1 : &ifreq0,
               sizeof(ifreq0));

    for (;;) {
        char buffer[96];
        struct sockaddr_storage src_addr;
        socklen_t src_addr_len = sizeof(src_addr);
        ssize_t count = recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr*)&src_addr, &src_addr_len);

        /* Drop messages that are too large to send a response in the buffer */
        if (count > 0 && count <= sizeof(buffer) - 16) {
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

            sendto(fd, buffer, reply_len, 0, (struct sockaddr*)&src_addr, src_addr_len);
        }
    }
}


void wificfg_init(uint32_t port, const wificfg_dispatch *dispatch)
{
    char *wifi_sta_ssid = NULL;
    char *wifi_sta_password = NULL;
    char *wifi_ap_ssid = NULL;
    char *wifi_ap_password = NULL;

    uint32_t base_addr;
    uint32_t num_sectors;
    if (sysparam_get_info(&base_addr, &num_sectors) != SYSPARAM_OK) {
        printf("Warning: WiFi config, sysparam not initialized\n");
        return;
    }

    /* Default a hostname. */
    char *hostname = NULL;
    sysparam_get_string("hostname", &hostname);
    if (!hostname && wificfg_default_hostname) {
        uint8_t macaddr[6];
        char name[32];
        sdk_wifi_get_macaddr(1, macaddr);
        snprintf(name, sizeof(name), wificfg_default_hostname, macaddr[3],
                 macaddr[4], macaddr[5]);
        sysparam_set_string("hostname", name);
    }
    if (hostname) {
        free(hostname);
    }

    sysparam_get_string("wifi_ap_ssid", &wifi_ap_ssid);
    sysparam_get_string("wifi_ap_password", &wifi_ap_password);
    sysparam_get_string("wifi_sta_ssid", &wifi_sta_ssid);
    sysparam_get_string("wifi_sta_password", &wifi_sta_password);

    int8_t wifi_sta_enable = 1;
    int8_t wifi_ap_enable = 1;
    sysparam_get_int8("wifi_sta_enable", &wifi_sta_enable);
    sysparam_get_int8("wifi_ap_enable", &wifi_ap_enable);

    int8_t wifi_sta_disabled_restarts = 0;
    sysparam_get_int8("wifi_sta_disabled_restarts", &wifi_sta_disabled_restarts);
    if (wifi_sta_disabled_restarts > 0) {
        wifi_sta_enable = 0;
        wifi_sta_disabled_restarts--;
        sysparam_set_int8("wifi_sta_disabled_restarts", wifi_sta_disabled_restarts);
    }

    int8_t wifi_ap_disabled_restarts = 0;
    sysparam_get_int8("wifi_ap_disabled_restarts", &wifi_ap_disabled_restarts);
    if (wifi_ap_disabled_restarts > 0) {
        wifi_ap_enable = 0;
        wifi_ap_disabled_restarts--;
        sysparam_set_int8("wifi_ap_disabled_restarts", wifi_ap_disabled_restarts);
    }

    /* Validate the configuration. */

    if (wifi_sta_enable && (!wifi_sta_ssid || !wifi_sta_password ||
                            strlen(wifi_sta_ssid) < 1 ||
                            strlen(wifi_sta_ssid) > 32 ||
                            !wifi_sta_password ||
                            strlen(wifi_sta_password) < 8 ||
                            strlen(wifi_sta_password) >= 64)) {
        wifi_sta_enable = 0;
    }

    if (wifi_ap_enable) {
        /* Default AP ssid and password. */
        if (!wifi_ap_ssid && wificfg_default_ssid) {
            uint8_t macaddr[6];
            char ssid[32];
            sdk_wifi_get_macaddr(1, macaddr);
            snprintf(ssid, sizeof(ssid), wificfg_default_ssid, macaddr[3],
                     macaddr[4], macaddr[5]);
            sysparam_set_string("wifi_ap_ssid", ssid);
            sysparam_get_string("wifi_ap_ssid", &wifi_ap_ssid);

            if (!wifi_ap_password && wificfg_default_password) {
                sysparam_set_string("wifi_ap_password", wificfg_default_password);
                sysparam_get_string("wifi_ap_password", &wifi_ap_password);
            }
        }

        /* If the ssid and password are not valid then disable the AP interface. */
        if (!wifi_ap_ssid || strlen(wifi_ap_ssid) < 1 || strlen(wifi_ap_ssid) >= 32 ||
            !wifi_ap_password || strlen(wifi_ap_password) < 8 || strlen(wifi_ap_password) >= 64) {
            wifi_ap_enable = 0;
        }
    }

    int8_t wifi_mode = NULL_MODE;
    if (wifi_sta_enable && wifi_ap_enable)
        wifi_mode = STATIONAP_MODE;
    else if (wifi_sta_enable)
        wifi_mode = STATION_MODE;
    else if (wifi_ap_enable)
        wifi_mode = SOFTAP_MODE;
    sdk_wifi_set_opmode(wifi_mode);

    if (wifi_sta_enable) {
        struct sdk_station_config config;
        strcpy((char *)config.ssid, wifi_sta_ssid);
        strcpy((char *)config.password, wifi_sta_password);
        config.bssid_set = 0;

        int8_t wifi_sta_dhcp = 1;
        sysparam_get_int8("wifi_sta_dhcp", &wifi_sta_dhcp);

        if (!wifi_sta_dhcp) {
            char *wifi_sta_ip_addr = NULL;
            char *wifi_sta_netmask = NULL;
            char *wifi_sta_gateway = NULL;
            sysparam_get_string("wifi_sta_ip_addr", &wifi_sta_ip_addr);
            sysparam_get_string("wifi_sta_netmask", &wifi_sta_netmask);
            sysparam_get_string("wifi_sta_gateway", &wifi_sta_gateway);

            if (wifi_sta_ip_addr && strlen(wifi_sta_ip_addr) > 4 &&
                wifi_sta_netmask && strlen(wifi_sta_netmask) > 4 &&
                wifi_sta_gateway && strlen(wifi_sta_gateway) > 4) {
                sdk_wifi_station_dhcpc_stop();
                struct ip_info info;
                memset(&info, 0x0, sizeof(info));
                info.ip.addr = ipaddr_addr(wifi_sta_ip_addr);
                info.netmask.addr = ipaddr_addr(wifi_sta_netmask);
                info.gw.addr = ipaddr_addr(wifi_sta_gateway);
                sdk_wifi_set_ip_info(STATION_IF, &info);
            }
            if (wifi_sta_ip_addr) free(wifi_sta_ip_addr);
            if (wifi_sta_netmask) free(wifi_sta_netmask);
            if (wifi_sta_gateway) free(wifi_sta_gateway);
        }

        sdk_wifi_station_set_config(&config);
    }

    if (wifi_ap_enable) {
        /* Read and validate paramenters. */
        int8_t wifi_ap_ssid_hidden = 0;
        sysparam_get_int8("wifi_ap_ssid_hidden", &wifi_ap_ssid_hidden);
        if (wifi_ap_ssid_hidden < 0 || wifi_ap_ssid_hidden > 1) {
            wifi_ap_ssid_hidden = 1;
        }

        int8_t wifi_ap_channel = 6;
        sysparam_get_int8("wifi_ap_channel", &wifi_ap_channel);

        /* AU does not allow channels above 13, although 14 works. */
        if (wifi_ap_channel > 13) {
            wifi_ap_channel = 13;
        }
#if 0
        /* US does not allow channels above 11, although they work. */
        if (wifi_ap_channel > 11) {
            wifi_ap_channel = 11;
        }
#endif
        if (wifi_ap_channel < 1 || wifi_ap_channel > 14) {
            wifi_ap_channel = 6;
        }

        int8_t wifi_ap_authmode = AUTH_WPA_WPA2_PSK;
        sysparam_get_int8("wifi_ap_authmode", &wifi_ap_authmode);
        if (wifi_ap_authmode != AUTH_OPEN && wifi_ap_authmode != AUTH_WPA_PSK &&
            wifi_ap_authmode != AUTH_WPA2_PSK && wifi_ap_authmode != AUTH_WPA_WPA2_PSK) {
            wifi_ap_authmode = AUTH_WPA_WPA2_PSK;
        }

        int8_t wifi_ap_max_conn = 3;
        sysparam_get_int8("wifi_ap_max_conn", &wifi_ap_max_conn);
        if (wifi_ap_max_conn < 1 || wifi_ap_max_conn > 8) {
            wifi_ap_max_conn = 3;
        }

        int32_t wifi_ap_beacon_interval = 100;
        sysparam_get_int32("wifi_ap_beacon_interval", &wifi_ap_beacon_interval);
        if (wifi_ap_beacon_interval < 0 || wifi_ap_beacon_interval > 1000) {
            wifi_ap_beacon_interval = 100;
        }

        /* Default AP IP address and netmask. */
        char *wifi_ap_ip_addr = NULL;
        sysparam_get_string("wifi_ap_ip_addr", &wifi_ap_ip_addr);
        if (!wifi_ap_ip_addr) {
            sysparam_set_string("wifi_ap_ip_addr", "172.16.0.1");
            sysparam_get_string("wifi_ap_ip_addr", &wifi_ap_ip_addr);
        }
        char *wifi_ap_netmask = NULL;
        sysparam_get_string("wifi_ap_netmask", &wifi_ap_netmask);
        if (!wifi_ap_netmask) {
            sysparam_set_string("wifi_ap_netmask", "255.255.0.0");
            sysparam_get_string("wifi_ap_netmask", &wifi_ap_netmask);
        }

        if (strlen(wifi_ap_ip_addr) >= 7 && strlen(wifi_ap_netmask) >= 7) {
            struct ip_info ap_ip;
            ap_ip.ip.addr = ipaddr_addr(wifi_ap_ip_addr);
            ap_ip.netmask.addr = ipaddr_addr(wifi_ap_netmask);
            IP4_ADDR(&ap_ip.gw, 0, 0, 0, 0);
            sdk_wifi_set_ip_info(1, &ap_ip);

            struct sdk_softap_config ap_config = {
                .ssid_hidden = wifi_ap_ssid_hidden,
                .channel = wifi_ap_channel,
                .authmode = wifi_ap_authmode,
                .max_connection = wifi_ap_max_conn,
                .beacon_interval = wifi_ap_beacon_interval,
            };
            strcpy((char *)ap_config.ssid, wifi_ap_ssid);
            ap_config.ssid_len = strlen(wifi_ap_ssid);
            strcpy((char *)ap_config.password, wifi_ap_password);
            sdk_wifi_softap_set_config(&ap_config);

            int8_t wifi_ap_dhcp_leases = 4;
            sysparam_get_int8("wifi_ap_dhcp_leases", &wifi_ap_dhcp_leases);

            if (wifi_ap_dhcp_leases) {
                ip4_addr_t first_client_ip;
                first_client_ip.addr = ap_ip.ip.addr + htonl(1);

                int8_t wifi_ap_dns = 1;
                sysparam_get_int8("wifi_ap_dns", &wifi_ap_dns);
                if (wifi_ap_dns < 0 || wifi_ap_dns > 1)
                    wifi_ap_dns = 1;

                dhcpserver_start(&first_client_ip, wifi_ap_dhcp_leases);
                dhcpserver_set_router(&ap_ip.ip);
                if (wifi_ap_dns) {
                    dhcpserver_set_dns(&ap_ip.ip);
                    xTaskCreate(dns_task, "WiFi Cfg DNS", 384, NULL, 2, NULL);
                }
            }
        }

        free(wifi_ap_ip_addr);
        free(wifi_ap_netmask);
    }

    if (wifi_sta_ssid) free(wifi_sta_ssid);
    if (wifi_sta_password) free(wifi_sta_password);
    if (wifi_ap_ssid) free(wifi_ap_ssid);
    if (wifi_ap_password) free(wifi_ap_password);

    if (wifi_mode != NULL_MODE) {
        server_params *params = malloc(sizeof(server_params));
        params->port = port;
        params->wificfg_dispatch = wificfg_dispatch_list;
        params->dispatch = dispatch;

        size_t stack_size = 464;
        xTaskCreate(server_task, "WiFi Cfg HTTP", stack_size, params, 2, NULL);
    }
}
