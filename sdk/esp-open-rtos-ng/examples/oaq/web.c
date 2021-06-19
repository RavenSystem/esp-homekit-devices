/*
 * Web interface.
 *
 * Copyright (C) 2016, 2017 OurAirQuality.org
 *
 * Licensed under the Apache License, Version 2.0, January 2004 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
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
#include "espressif/esp_common.h"

#include <unistd.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "ds3231/ds3231.h"

#include "buffer.h"
#include "pms.h"
#include "config.h"
#include "flash.h"
#include "sha3.h"
#include "ds3231.h"
#include "sht21.h"
#include "bme280.h"
#include "bmp180.h"
#include "i2c.h"
#include "leds.h"

#include "config.h"
#include "wificfg/wificfg.h"
#include "sysparam.h"


static const char http_success_header[] = "HTTP/1.1 200 \r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Cache-Control: no-store\r\n"
    "Transfer-Encoding: chunked\r\n"
    "Connection: close\r\n"
    "\r\n";

static const char *http_index_content[] = {
#include "content/index.html"
};

static int handle_index(int s, wificfg_method method,
                        uint32_t content_length,
                        wificfg_content_type content_type,
                        char *buf, size_t len)
{
    if (wificfg_write_string(s, http_success_header) < 0) return -1;
    
    if (method != HTTP_METHOD_HEAD) {
        if (wificfg_write_string_chunk(s, http_index_content[0], buf, len) < 0) return -1;
        if (wificfg_write_html_title(s, buf, len, "Home") < 0) return -1;
        if (wificfg_write_string_chunk(s, http_index_content[1], buf, len) < 0) return -1;

        if (wificfg_write_string_chunk(s, "<dl class=\"dlh\">", buf, len) < 0) return -1;

        {
            char *hostname = NULL;
            sysparam_get_string("hostname", &hostname);
            if (!hostname) {
                sysparam_get_string("wifi_ap_ssid", &hostname);
            }
            if (hostname) {
                if (wificfg_write_string_chunk(s, "<dt>Name</dt><dd>", buf, len) < 0) {
                    free(hostname);
                    return -1;
                }
                wificfg_html_escape(hostname, buf, len);
                free(hostname);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                if (wificfg_write_string_chunk(s, "</dd>", buf, len) < 0) return -1;
            }
        }

        int8_t logging = get_buffer_logging();
        if (logging) {
            if (wificfg_write_string_chunk(s, "<dt>Logging is enabled</dt><dd><form action=\"/logging.html\" method=\"post\"\"><button name=\"oaq_logging\" type=\"submit\" value=\"0\">Pause logging</button><input type=\"hidden\" name=\"done\"></form></dd>", buf, len) < 0) return -1;
        } else {
            if (wificfg_write_string_chunk(s, "<dt>Logging is paused</dt><dd><form action=\"/logging.html\" method=\"post\"\"><button name=\"oaq_logging\" type=\"submit\" value=\"1\">Restart logging</button><input type=\"hidden\" name=\"done\"></form></dd>", buf, len) < 0) return -1;
        }

        uint32_t index, next_index;
        bool sealed;
        uint32_t size = get_buffer_size(0xffffffff, &index, &next_index, &sealed);
        snprintf(buf, len, "<dt>Flash sector</dt><dd>index %u, size %u</dd>", index, size);
        if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;

        if (wificfg_write_string_chunk(s, "<dt>Last measured data:</dt><dd></dd>", buf, len) < 0) return -1;

        {
            uint32_t counter;
            struct tm time;
            float temp;

            if (ds3231_time_temp(&counter, &time, &temp)) {
                /* Apply the time zone. */
                int8_t tz = 0;
                sysparam_get_int8("oaq_tz", &tz);
                time_t clock_time = mktime(&time);
                clock_time -= tz * 60 * 60;
                gmtime_r(&clock_time, &time);

                if (wificfg_write_string_chunk(s, "<dt>DS3231</dt>", buf, len) < 0) return -1;
                const char *wday[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
                snprintf(buf, len, "<dd>%02u:%02u:%02u %s %u/%u/%u", time.tm_hour, time.tm_min, time.tm_sec, wday[time.tm_wday], time.tm_mday, time.tm_mon + 1, time.tm_year + 1900);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                snprintf(buf, len, ", %.1f Deg&nbsp;C</dd>", temp);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
            }
        }

        {
            uint32_t counter;
            float temp, rh;
            if (sht2x_temp_rh(&counter, &temp, &rh)) {
                if (wificfg_write_string_chunk(s, "<dt>SHT2x</dt>", buf, len) < 0) return -1;
                snprintf(buf, len, "<dd>%.1f Deg&nbsp;C, %.1f&nbsp;%% RH</dd>", temp, rh);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
            }
        }

        {
            uint32_t counter;
            float temp, press, rh;
            if (bme280_temp_press_rh(&counter, &temp, &press, &rh)) {
                if (wificfg_write_string_chunk(s, "<dt>BME280</dt>", buf, len) < 0) return -1;
                snprintf(buf, len, "<dd>%.1f Deg&nbsp;C, %.0f Pa", temp, press);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                snprintf(buf, len, ", %.1f&nbsp;%% RH</dd>", rh);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
            }
        }

        {
            uint32_t counter;
            float temp, press;
            if (bmp180_temp_press(&counter, &temp, &press)) {
                if (wificfg_write_string_chunk(s, "<dt>BMP180</dt>", buf, len) < 0) return -1;
                snprintf(buf, len, "<dd>%.1f Deg&nbsp;C, %.0f Pa</dd>", temp, press);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
            }
        }

        {
            uint32_t counter;
            uint16_t pm1a = 0;
            uint16_t pm25a = 0;
            uint16_t pm10a = 0;
            uint16_t pm1b = 0;
            uint16_t pm25b = 0;
            uint16_t pm10b = 0;
            uint16_t c1 = 0;
            uint16_t c2 = 0;
            uint16_t c3 = 0;
            uint16_t c4 = 0;
            uint16_t c5 = 0;
            uint16_t c6 = 0;
            uint16_t r1 = 0;

            if (pms_last_data(&counter, &pm1a, &pm25a, &pm10a, &pm1b, &pm25b, &pm10b, &c1, &c2, &c3, &c4, &c5, &c6, &r1)) {
                if (wificfg_write_string_chunk(s, "<dt>PM1.0</dt>", buf, len) < 0) return -1;
                snprintf(buf, len, "<dd>%u / %u</dd>", pm1a, pm1b);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                if (wificfg_write_string_chunk(s, "<dt>PM2.5</dt>", buf, len) < 0) return -1;
                snprintf(buf, len, "<dd>%u / %u</dd>", pm25a, pm25b);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                if (wificfg_write_string_chunk(s, "<dt>PM10</dt>", buf, len) < 0) return -1;
                snprintf(buf, len, "<dd>%u / %u</dd>", pm10a, pm10b);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;

                if (wificfg_write_string_chunk(s, "<dt>0.3&#x00b5;m</dt>", buf, len) < 0) return -1;
                snprintf(buf, len, "<dd>%u</dd>", c1);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                if (wificfg_write_string_chunk(s, "<dt>0.5&#x00b5;m</dt>", buf, len) < 0) return -1;
                snprintf(buf, len, "<dd>%u</dd>", c2);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                if (wificfg_write_string_chunk(s, "<dt>1.0&#x00b5;m</dt>", buf, len) < 0) return -1;
                snprintf(buf, len, "<dd>%u</dd>", c3);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                if (wificfg_write_string_chunk(s, "<dt>2.5&#x00b5;m</dt>", buf, len) < 0) return -1;
                snprintf(buf, len, "<dd>%u</dd>", c4);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                if (wificfg_write_string_chunk(s, "<dt>5.0&#x00b5;m</dt>", buf, len) < 0) return -1;
                snprintf(buf, len, "<dd>%u</dd>", c5);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                if (wificfg_write_string_chunk(s, "<dt>10&#x00b5;m</dt>", buf, len) < 0) return -1;
                snprintf(buf, len, "<dd>%u</dd>", c6);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;

                if (wificfg_write_string_chunk(s, "<dt>Version</dt>", buf, len) < 0) return -1;
                snprintf(buf, len, "<dd>%u</dd>", r1 >> 8);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                if (wificfg_write_string_chunk(s, "<dt>Error code</dt>", buf, len) < 0) return -1;
                snprintf(buf, len, "<dd>%u</dd>", r1 & 0xff);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
            }
        }

        if (wificfg_write_string_chunk(s, "</dl>", buf, len) < 0) return -1;

        if (wificfg_write_string_chunk(s, http_index_content[2], buf, len) < 0) return -1;

        if (wificfg_write_chunk_end(s) < 0) return -1;
    }
    return 0;
}


static const char *http_config_content[] = {
#include "content/config.html"
};

static char base64codes[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

/* Emit a new line after 19 * 4 (76) characters. */
static int write_base64_chunked(int s, uint8_t *in, size_t len)
{
    int i;
    int line = 0;
    char buf[3 + 4 + 2];
    buf[0] = '4';
    buf[1] = '\r';
    buf[2] = '\n';
    buf[7] = '\r';
    buf[8] = '\n';
    for (i = 0; i < len; i += 3)  {
        if (line >= 76) {
            int count = write(s, "1\r\n\n\r\n", 6);
            if (count < 6)
                return count;
            line = 0;
        }
        buf[3] = base64codes[(in[i] & 0xFC) >> 2];
        uint8_t b = (in[i] & 0x03) << 4;
        if (i + 1 < len) {
            b |= (in[i + 1] & 0xF0) >> 4;
            buf[4] = base64codes[b];
            b = (in[i + 1] & 0x0F) << 2;
            if (i + 2 < len)  {
                b |= (in[i + 2] & 0xC0) >> 6;
                buf[5] = base64codes[b];
                b = in[i + 2] & 0x3F;
                buf[6] = base64codes[b];
            } else  {
                buf[5] = base64codes[b];
                buf[6] = '=';
            }
        } else      {
            buf[4] = base64codes[b];
            buf[5] = '=';
            buf[6] = '=';
        }
        int count = write(s, buf, 9);
        if (count < 9)
            return count;
        line += 4;
    }
    return len;
}

static int handle_config(int s, wificfg_method method,
                         uint32_t content_length,
                         wificfg_content_type content_type,
                         char *buf, size_t len)
{
    if (wificfg_write_string(s, http_success_header) < 0) return -1;

    if (method != HTTP_METHOD_HEAD) {
        if (wificfg_write_string_chunk(s, http_config_content[0], buf, len) < 0) return -1;
        if (wificfg_write_html_title(s, buf, len, "Sensor config") < 0) return -1;
        if (wificfg_write_string_chunk(s, http_config_content[1], buf, len) < 0) return -1;

        int8_t leds = 1; /* Nodemcu */
        sysparam_get_int8("oaq_leds", &leds);
        if (leds == 0 && wificfg_write_string_chunk(s, " selected", buf, len) < 0) return -1;
        if (wificfg_write_string_chunk(s, http_config_content[2], buf, len) < 0) return -1;
        if (leds == 1 && wificfg_write_string_chunk(s, " selected", buf, len) < 0) return -1;
        if (wificfg_write_string_chunk(s, http_config_content[3], buf, len) < 0) return -1;
        if (leds == 2 && wificfg_write_string_chunk(s, " selected", buf, len) < 0) return -1;
        if (wificfg_write_string_chunk(s, http_config_content[4], buf, len) < 0) return -1;

        int8_t pms_uart = 2; /* Enabled, RX */
        sysparam_get_int8("oaq_pms_uart", &pms_uart);
        if (pms_uart == 0 && wificfg_write_string_chunk(s, " selected", buf, len) < 0) return -1;
        if (wificfg_write_string_chunk(s, http_config_content[5], buf, len) < 0) return -1;
        if (pms_uart == 1 && wificfg_write_string_chunk(s, " selected", buf, len) < 0) return -1;
        if (wificfg_write_string_chunk(s, http_config_content[6], buf, len) < 0) return -1;
        if (pms_uart == 2 && wificfg_write_string_chunk(s, " selected", buf, len) < 0) return -1;
        if (wificfg_write_string_chunk(s, http_config_content[7], buf, len) < 0) return -1;

        int8_t i2c_scl = 5;
        sysparam_get_int8("oaq_i2c_scl", &i2c_scl);
        snprintf(buf, len, "%u", i2c_scl);
        if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;

        if (wificfg_write_string_chunk(s, http_config_content[8], buf, len) < 0) return -1;

        int8_t i2c_sda = 4;
        sysparam_get_int8("oaq_i2c_sda", &i2c_sda);
        snprintf(buf, len, "%u", i2c_sda);
        if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;

        if (wificfg_write_string_chunk(s, http_config_content[9], buf, len) < 0) return -1;

        int8_t tz = 0;
        sysparam_get_int8("oaq_tz", &tz);
        snprintf(buf, len, "%d", tz);
        if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;

        if (wificfg_write_string_chunk(s, http_config_content[10], buf, len) < 0) return -1;

        int8_t logging = 1;
        sysparam_get_int8("oaq_logging", &logging);
        if (logging && wificfg_write_string_chunk(s, "checked", buf, len) < 0) return -1;
        if (wificfg_write_string_chunk(s, http_config_content[11], buf, len) < 0) return -1;

        char *web_server = NULL;
        sysparam_get_string("oaq_web_server", &web_server);
        if (web_server) {
            wificfg_html_escape(web_server, buf, len);
            free(web_server);
            if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
        }

        if (wificfg_write_string_chunk(s, http_config_content[12], buf, len) < 0) return -1;

        int32_t web_port = 80;
        sysparam_get_int32("oaq_web_port", &web_port);
        snprintf(buf, len, "%u", web_port);
        if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;

        if (wificfg_write_string_chunk(s, http_config_content[13], buf, len) < 0) return -1;

        char *web_path = NULL;
        sysparam_get_string("oaq_web_path", &web_path);
        if (web_path) {
            wificfg_html_escape(web_path, buf, len);
            free(web_path);
        } else {
            wificfg_html_escape("/cgi-bin/recv", buf, len);
        }
        if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;

        if (wificfg_write_string_chunk(s, http_config_content[14], buf, len) < 0) return -1;

        int32_t sensor_id = 0;
        if (sysparam_get_int32("oaq_sensor_id", &sensor_id) == SYSPARAM_OK) {
            snprintf(buf, len, "%u", sensor_id);
            if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
        }

        if (wificfg_write_string_chunk(s, http_config_content[15], buf, len) < 0) return -1;

        uint8_t *sha3_key = NULL;
        size_t actual_length;
        if (sysparam_get_data("oaq_sha3_key", &sha3_key, &actual_length, NULL) == SYSPARAM_OK) {
            if (sha3_key) {
                if (write_base64_chunked(s, sha3_key, actual_length) < actual_length) {
                    free(sha3_key);
                    return -1;
                }
                free(sha3_key);
            }
        }

        if (wificfg_write_string_chunk(s, http_config_content[16], buf, len) < 0) return -1;

        struct tm time;
        xSemaphoreTake(i2c_sem, portMAX_DELAY);
        bool ds3231_available = ds3231_getTime(&ds3231_dev, &time);
        xSemaphoreGive(i2c_sem);

        if (ds3231_available) {
            /* Apply the time zone. */
            time_t clock_time = mktime(&time);
            clock_time -= tz * 60 * 60;
            gmtime_r(&clock_time, &time);

            if (wificfg_write_string_chunk(s, http_config_content[17], buf, len) < 0) return -1;

            snprintf(buf, len, "%u", time.tm_year + 1900);
            if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;

            if (wificfg_write_string_chunk(s, http_config_content[18], buf, len) < 0) return -1;

            snprintf(buf, len, "%u", time.tm_mon + 1);
            if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;

            if (wificfg_write_string_chunk(s, http_config_content[19], buf, len) < 0) return -1;

            snprintf(buf, len, "%u", time.tm_mday);
            if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;

            if (wificfg_write_string_chunk(s, http_config_content[20], buf, len) < 0) return -1;

            snprintf(buf, len, "%u", time.tm_hour);
            if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;

            if (wificfg_write_string_chunk(s, http_config_content[21], buf, len) < 0) return -1;

            snprintf(buf, len, "%u", time.tm_min);
            if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;

            if (wificfg_write_string_chunk(s, http_config_content[22], buf, len) < 0) return -1;

            snprintf(buf, len, "%u", time.tm_sec);
            if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;

            if (wificfg_write_string_chunk(s, http_config_content[23], buf, len) < 0) return -1;
        }

        /* Erase flash */
        if (wificfg_write_string_chunk(s, http_config_content[24], buf, len) < 0) return -1;

        if (wificfg_write_string_chunk(s, http_config_content[25], buf, len) < 0) return -1;

        if (wificfg_write_chunk_end(s) < 0) return -1;
    }
    return 0;
}

typedef enum {
    FORM_NAME_LEDS,
    FORM_NAME_PMS_UART,
    FORM_NAME_I2C_SCL,
    FORM_NAME_I2C_SDA,
    FORM_NAME_TZ,
    FORM_NAME_LOGGING,
    FORM_NAME_WEB_SERVER,
    FORM_NAME_WEB_PORT,
    FORM_NAME_WEB_PATH,
    FORM_NAME_SENSOR_ID,
    FORM_NAME_SHA3_KEY,
    FORM_NAME_YEAR,
    FORM_NAME_MONTH,
    FORM_NAME_MDAY,
    FORM_NAME_HOUR,
    FORM_NAME_MIN,
    FORM_NAME_SEC,
    FORM_NAME_UTIMEH,
    FORM_NAME_UTIMEL,
    FORM_NAME_MESSAGE,
    FORM_NAME_INDEX,
    FORM_NAME_START,
    FORM_NAME_END,
    FORM_NAME_DONE,
    FORM_NAME_NONE
} form_name;

static const struct {
    const char *str;
    form_name name;
} form_name_table[] = {
    {"oaq_leds", FORM_NAME_LEDS},
    {"oaq_pms_uart", FORM_NAME_PMS_UART},
    {"oaq_i2c_scl", FORM_NAME_I2C_SCL},
    {"oaq_i2c_sda", FORM_NAME_I2C_SDA},
    {"oaq_tz", FORM_NAME_TZ},
    {"oaq_logging", FORM_NAME_LOGGING},
    {"oaq_web_server", FORM_NAME_WEB_SERVER},
    {"oaq_web_port", FORM_NAME_WEB_PORT},
    {"oaq_web_path", FORM_NAME_WEB_PATH},
    {"oaq_sensor_id", FORM_NAME_SENSOR_ID},
    {"oaq_sha3_key", FORM_NAME_SHA3_KEY},
    {"oaq_year", FORM_NAME_YEAR},
    {"oaq_month", FORM_NAME_MONTH},
    {"oaq_mday", FORM_NAME_MDAY},
    {"oaq_hour", FORM_NAME_HOUR},
    {"oaq_min", FORM_NAME_MIN},
    {"oaq_sec", FORM_NAME_SEC},
    {"oaq_utimeh", FORM_NAME_UTIMEH},
    {"oaq_utimel", FORM_NAME_UTIMEL},
    {"oaq_message", FORM_NAME_MESSAGE},
    {"oaq_index", FORM_NAME_INDEX},
    {"oaq_start", FORM_NAME_START},
    {"oaq_end", FORM_NAME_END},
    {"done", FORM_NAME_DONE},
};

static form_name intern_form_name(char *str)
{
     int i;
     for (i = 0;  i < sizeof(form_name_table) / sizeof(form_name_table[0]); i++) {
         if (!strcmp(str, form_name_table[i].str))
             return form_name_table[i].name;
     }
     return FORM_NAME_NONE;
}

static const char http_home_redirect[] = "HTTP/1.1 302 \r\n"
    "Location: /\r\n"
    "Content-Length: 0\r\n"
    "Connection: close\r\n"
    "\r\n";

static int handle_logging_post(int s, wificfg_method method,
                               uint32_t content_length,
                               wificfg_content_type content_type,
                               char *buf, size_t len)
{
    if (content_type != HTTP_CONTENT_TYPE_WWW_FORM_URLENCODED) {
        return wificfg_write_string(s, "HTTP/1.1 400 \r\n"
                                    "Content-Type: text/html\r\n"
                                    "Content-Length: 0\r\n"
                                    "Connection: close\r\n"
                                    "\r\n");
    }

    size_t rem = content_length;
    bool valp = false;

    /* Delay committing some values until all have been read. */
    bool done = false;
    int8_t logging = 0;

    while (rem > 0) {
        int r = wificfg_form_name_value(s, &valp, &rem, buf, len);

        if (r < 0)
            break;

        wificfg_form_url_decode(buf);

        form_name name = intern_form_name(buf);

        if (valp) {
            int r = wificfg_form_name_value(s, NULL, &rem, buf, len);
            if (r < 0)
                break;

            wificfg_form_url_decode(buf);

            switch (name) {
            case FORM_NAME_LOGGING: {
                logging = strtoul(buf, NULL, 10) != 0;
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
        set_buffer_logging(logging);
    }

    return wificfg_write_string(s, http_home_redirect);
}


static const char http_config_redirect_header[] = "HTTP/1.1 302 \r\n"
    "Location: /config.html\r\n"
    "Content-Length: 0\r\n"
    "Connection: close\r\n"
    "\r\n";

static const uint8_t base64table[256] =
{
    65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
    65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
    65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 62, 65, 65, 65, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 65, 65, 65, 64, 65, 65,
    65,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 65, 65, 65, 65, 65,
    65, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 65, 65, 65, 65, 65,
    65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
    65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
    65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
    65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
    65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
    65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
    65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
    65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65
};

/*
 * Decode a form-url encoded value with base64 encoded binary data.
 *
 * TODO this does not validate and consume the end properly.
 */
static uint8_t *read_base64(int s, int len, size_t *rem)
{
    if (len == 0)
        return NULL;

    uint8_t *decoded = malloc(len);
    if (!decoded)
        return NULL;

    int j = 0;
    while (*rem > 0) {
        /* Read four characters ignoring noise */
        uint8_t b[4];
        int n = 0;
        while (rem > 0) {
            char c;
            int r = read(s, &c, 1);
            /* Expecting a known number of characters so fail on EOF. */
            if (r < 1) {
                free(decoded);
                return NULL;
            }
            (*rem)--;
            if (c == '%') {
                if (*rem < 2) {
                    free(decoded);
                    return NULL;
                }
                unsigned char c2[2];
                int r = read(s, &c2, 2);
                if (r < 0) {
                    free(decoded);
                    return NULL;
                }
                (*rem) -= r;
                if (r < 2) {
                    free(decoded);
                    return NULL;
                }
                if (isxdigit(c2[0]) && isxdigit(c2[1])) {
                    c2[0] = tolower(c2[0]);
                    int d1 = (c2[0] >= 'a' && c2[0] <= 'z') ? c2[0] - 'a' + 10 : c2[0] - '0';
                    c2[1] = tolower(c2[1]);
                    int d2 = (c2[1] >= 'a' && c2[1] <= 'z') ? c2[1] - 'a' + 10 : c2[1] - '0';
                    int v = base64table[(d1 << 4) + d2];
                    if (v < 65) {
                        b[n++] = v;
                        if (n >= 4)
                            break;
                    }
                }
            } else if (c == '&') {
                /* Ended early. */
                free(decoded);
                return NULL;
            } else {
                int v = base64table[(int)c];
                if (v < 65) {
                    b[n++] = v;
                    if (n >= 4)
                        break;
                }
            }
        }

        decoded[j++] = (b[0] << 2) | (b[1] >> 4);
        if (j >= len)
            return decoded;
        if (b[2] < 64) {
            decoded[j++] = (b[1] << 4) | (b[2] >> 2);
            if (j >= len)
                return decoded;
            if (b[3] < 64) {
                decoded[j++] = (b[2] << 6) | b[3];
                if (j >= len)
                    return decoded;
            }
        }
    }

    /* Ran out of input to fill the requested length */
    free(decoded);
    return NULL;
}

static int handle_config_post(int s, wificfg_method method,
                              uint32_t content_length,
                              wificfg_content_type content_type,
                              char *buf, size_t len)
{
    if (content_type != HTTP_CONTENT_TYPE_WWW_FORM_URLENCODED) {
        return wificfg_write_string(s, "HTTP/1.1 400 \r\n"
                                    "Content-Type: text/html\r\n"
                                    "Content-Length: 0\r\n"
                                    "Connection: close\r\n"
                                    "\r\n");
    }

    size_t rem = content_length;
    bool valp = false;

    /* Delay committing some values until all have been read. */
    bool done = false;
    int8_t logging = 0;

    while (rem > 0) {
        int r = wificfg_form_name_value(s, &valp, &rem, buf, len);

        if (r < 0)
            break;

        wificfg_form_url_decode(buf);

        form_name name = intern_form_name(buf);

        if (valp) {
            if (name == FORM_NAME_SHA3_KEY) {
                uint8_t *key = read_base64(s, 287, &rem);
                // TODO if there were not the last value then it
                // would be necessary to skip to the '&' character.
                if (key) {
                    sysparam_set_data("oaq_sha3_key", key, 287, true);
                    free(key);
                }
            } else {
                int r = wificfg_form_name_value(s, NULL, &rem, buf, len);
                if (r < 0)
                    break;

                wificfg_form_url_decode(buf);

                switch (name) {
                case FORM_NAME_LEDS: {
                    int8_t leds = strtoul(buf, NULL, 10);
                    if (leds >= 0 && leds <= 2) {
                        sysparam_set_int8("oaq_leds", leds);
                        /* Apply this now. */
                        param_leds = leds;
                        init_blink();
                    }
                    break;
                }
                case FORM_NAME_PMS_UART: {
                    int8_t uart = strtoul(buf, NULL, 10);
                    if (uart >= 0 && uart <= 2)
                        sysparam_set_int8("oaq_pms_uart", uart);
                    break;
                }
                case FORM_NAME_I2C_SCL: {
                    int8_t i2c_scl = strtoul(buf, NULL, 10);
                    if (i2c_scl >= 0 && i2c_scl <= 15)
                        sysparam_set_int8("oaq_i2c_scl", i2c_scl);
                    break;
                }
                case FORM_NAME_I2C_SDA: {
                    int8_t i2c_sda = strtoul(buf, NULL, 10);
                    if (i2c_sda >= 0 && i2c_sda <= 15)
                        sysparam_set_int8("oaq_i2c_sda", i2c_sda);
                    break;
                }
                case FORM_NAME_TZ: {
                    int32_t tz = strtol(buf, NULL, 10);
                    if (tz >= -12 && tz <= 12)
                        sysparam_set_int8("oaq_tz", tz);
                    break;
                }
                case FORM_NAME_LOGGING: {
                    logging = strtoul(buf, NULL, 10) != 0;
                    break;
                }
                case FORM_NAME_WEB_SERVER: {
                    sysparam_set_string("oaq_web_server", buf);
                    break;
                }
                case FORM_NAME_WEB_PORT: {
                    int32_t port = strtoul(buf, NULL, 10);
                    if (port >= 0 && port <= 65535)
                        sysparam_set_int32("oaq_web_port", port);
                    break;
                }
                case FORM_NAME_WEB_PATH: {
                    sysparam_set_string("oaq_web_path", buf);
                    break;
                }
                case FORM_NAME_SENSOR_ID: {
                    int32_t id = strtoul(buf, NULL, 10);
                    sysparam_set_int32("oaq_sensor_id", id);
                    break;
                }
                case FORM_NAME_DONE:
                    done = true;
                    break;
                case FORM_NAME_SHA3_KEY:
                default:
                    break;
                }
            }
        }
    }

    if (done) {
        /* Just change the 'startup' flag, not the running state. */
        sysparam_set_int8("oaq_logging", logging);
    }

    return wificfg_write_string(s, http_config_redirect_header);
}

static int handle_time_post(int s, wificfg_method method,
                            uint32_t content_length,
                            wificfg_content_type content_type,
                            char *buf, size_t len)
{
    if (content_type != HTTP_CONTENT_TYPE_WWW_FORM_URLENCODED) {
        return wificfg_write_string(s, "HTTP/1.1 400 \r\n"
                                    "Content-Type: text/html\r\n"
                                    "Content-Length: 0\r\n"
                                    "Connection: close\r\n"
                                    "\r\n");
    }

    size_t rem = content_length;
    bool valp = false;
    uint32_t utimeh = 0, utimel = 0;
    bool utimehp = false, utimelp = false;
    int32_t tz = 0;

    struct tm time;
    xSemaphoreTake(i2c_sem, portMAX_DELAY);
    bool ds3231_available = ds3231_getTime(&ds3231_dev, &time);
    xSemaphoreGive(i2c_sem);

    while (rem > 0) {
        int r = wificfg_form_name_value(s, &valp, &rem, buf, len);

        if (r < 0)
            break;

        wificfg_form_url_decode(buf);

        form_name name = intern_form_name(buf);

        if (valp) {
            int r = wificfg_form_name_value(s, NULL, &rem, buf, len);
            if (r < 0)
                break;

            wificfg_form_url_decode(buf);

            switch (name) {
            case FORM_NAME_YEAR: {
                int32_t year = strtoul(buf, NULL, 10);
                if (year >= 2000 && year <= 2099)
                    time.tm_year = year - 1900;
                break;
            }
            case FORM_NAME_MONTH: {
                int32_t month = strtoul(buf, NULL, 10);
                if (month >= 1 && month <= 12)
                    time.tm_mon = month - 1;
                break;
            }
            case FORM_NAME_MDAY: {
                int32_t mday = strtoul(buf, NULL, 10);
                if (mday >= 1 && mday <= 31)
                    time.tm_mday = mday;
                break;
            }
            case FORM_NAME_HOUR: {
                int32_t hour = strtoul(buf, NULL, 10);
                if (hour >= 0 && hour <= 32)
                    time.tm_hour = hour;
                break;
            }
            case FORM_NAME_MIN: {
                int32_t min = strtoul(buf, NULL, 10);
                if (min >= 0 && min <= 59)
                    time.tm_min = min;
                break;
            }
            case FORM_NAME_SEC: {
                int32_t sec = strtoul(buf, NULL, 10);
                if (sec >= 0 && sec <= 59)
                    time.tm_sec = sec;
                break;
            }
            case FORM_NAME_UTIMEH: {
                utimeh = strtoul(buf, NULL, 10);
                utimehp = true;
                break;
            }
            case FORM_NAME_UTIMEL: {
                utimel = strtoul(buf, NULL, 10);
                utimelp = true;
                break;
            }
            case FORM_NAME_TZ: {
                tz = strtol(buf, NULL, 10);
                break;
            }
            default:
                break;
            }
        }
    }

    if (ds3231_available) {
        /* Note the original values of tm_wday and tm_yday are ignored by mktime */
        time_t clock_time = mktime(&time);

        if (utimehp && utimelp && utimeh > 0) {
            /* Synchronising to the client time. */
            uint64_t utime = (uint64_t)utimeh << 32 | utimel;

            /* Round off to the nearest second. */
            utime = (utime + 500UL) / 1000UL;

            /* Quick validity check. */
            if (utime > 1502090000UL) {
                /* If the rtc clock is behind the client clock then adopt it. This
                 * could be repeated a few times and the post with the smallest
                 * delay will have been used.
                 *
                 * If the rtc clock is ahead by more than the expected network delay
                 * then also adopt it. This leaves a small window in which the clock
                 * might be slightly ahead.
                 */
                if (clock_time < utime || clock_time > utime + 4) {
                    clock_time = utime;
                    gmtime_r(&clock_time, &time);
                    xSemaphoreTake(i2c_sem, portMAX_DELAY);
                    ds3231_setTime(&ds3231_dev, &time);
                    xSemaphoreGive(i2c_sem);
                    if (tz >= -12 && tz <= 12) {
                        sysparam_set_int8("oaq_tz", tz);
                    }
                }
            }
        } else {
            /* Apply the time zone. */
            int8_t tz = 0;
            sysparam_get_int8("oaq_tz", &tz);
            clock_time += tz * 60 * 60;
            gmtime_r(&clock_time, &time);

            xSemaphoreTake(i2c_sem, portMAX_DELAY);
            ds3231_setTime(&ds3231_dev, &time);
            xSemaphoreGive(i2c_sem);
        }
    }

    return wificfg_write_string(s, http_config_redirect_header);
}

static int handle_erase_data_post(int s, wificfg_method method,
                                  uint32_t content_length,
                                  wificfg_content_type content_type,
                                  char *buf, size_t len)
{
    if (content_type != HTTP_CONTENT_TYPE_WWW_FORM_URLENCODED) {
        return wificfg_write_string(s, "HTTP/1.1 400 \r\n"
                                    "Content-Type: text/html\r\n"
                                    "Content-Length: 0\r\n"
                                    "Connection: close\r\n"
                                    "\r\n");
    }

    if (!erase_flash_data()) {
        printf("Warning: error erasing data flash?\n");
    }

    wificfg_write_string(s, http_config_redirect_header);
    return 0;
}


static const char *http_smoothie[] = {
#include "content/smoothie.js"
};

static int handle_smoothie(int s, wificfg_method method,
                           uint32_t content_length,
                           wificfg_content_type content_type,
                           char *buf, size_t len)
{
    if (wificfg_write_string(s, http_smoothie[0]) < 0) return -1;

    if (method != HTTP_METHOD_HEAD) {
        if (wificfg_write_string_chunk(s, http_smoothie[1], buf, len) < 0) return -1;
        if (wificfg_write_chunk_end(s) < 0) return -1;
    }
    return 0;
}

static const char *http_plot_script[] = {
#include "content/plot.js"
};

static int handle_plot_script(int s, wificfg_method method,
                              uint32_t content_length,
                              wificfg_content_type content_type,
                              char *buf, size_t len)
{
    if (wificfg_write_string(s, http_plot_script[0]) < 0) return -1;

    if (method != HTTP_METHOD_HEAD) {
        if (wificfg_write_string_chunk(s, http_plot_script[1], buf, len) < 0) return -1;
        if (wificfg_write_chunk_end(s) < 0) return -1;
    }
    return 0;
}

static const char *http_plot_content[] = {
#include "content/plot.html"
};

static int handle_plot(int s, wificfg_method method,
                       uint32_t content_length,
                       wificfg_content_type content_type,
                       char *buf, size_t len)
{
    if (wificfg_write_string(s, http_success_header) < 0) return -1;

    if (method != HTTP_METHOD_HEAD) {
        if (wificfg_write_string_chunk(s, http_plot_content[0], buf, len) < 0) return -1;
        if (wificfg_write_html_title(s, buf, len, "Plot") < 0) return -1;
        if (wificfg_write_string_chunk(s, http_plot_content[1], buf, len) < 0) return -1;

        if (wificfg_write_chunk_end(s) < 0) return -1;
    }
    return 0;
}

static uint64_t last_logged_client_utime = 0;
static uint32_t last_client_utime_segment = 0;

/* The utime is a time from a web browser Date.now(), so in milli-seconds. */
void log_client_utime(uint32_t utimeh, uint32_t utimel)
{
    uint64_t utime = ((uint64_t)utimeh << 32) | utimel;

    uint8_t outbuf[10];
    while (1) {
        /* Delta encoding. Handle increases and decreases in time, in case
         * the client has adjusted time or conflicting client times. */
        int64_t utime_delta = utime - last_logged_client_utime;

        /* Except for the first utime in a segment or backwards steps, do not
         * log more than once every 10 minutes. */
        if (last_logged_client_utime > 0 && utime_delta > 0 &&
            utime_delta < 10 * 60 * 1000) {
            break;
        }

        uint32_t len = emit_leb128_signed(outbuf, 0, utime_delta);
        /* Flag this for high precision time (no truncation) */
        uint32_t new_segment = dbuf_append(last_client_utime_segment,
                                           DBUF_EVENT_CLIENT_UTIME,
                                           outbuf, len, 0);
        if (new_segment == last_client_utime_segment) {
            /*
             * Commit the values logged. Note this is the only task accessing
             * this state so these updates are synchronized with the last event
             * of this class append.
             */
            last_logged_client_utime = utime;
            break;
        }

        /* Moved on to a new buffer. Reset the delta encoding
         * state and retry. */
        last_client_utime_segment = new_segment;
        last_logged_client_utime = 0;
    }

}

/*
 * Log a text string with the given length (so it could be a binary
 * blob) and the client utime passed along with it. The string may be
 * NULL or of zero length, and the event is still logged, this allows
 * a quick ping event to be logged.
 *
 * This shares the same utime delta encoding as the utime event. Both
 * are called from the same web server thread so there is no need for
 * locking.
 */
void log_client_text_message(uint32_t utimeh, uint32_t utimel, char *str, size_t str_len)
{
    if (!str || str_len == 0) {
        /* Canonicalize these */
        str_len = 0;
        str = NULL;
    } else if (str_len > 50) {
        /* Limit the logged length? */
        str_len = 50;
    }

    uint64_t utime = ((uint64_t)utimeh << 32) | utimel;


    uint8_t *outbuf;
    outbuf = malloc(10 + 4 + str_len); // ??
    if (!outbuf) {
        return;
    }

    while (1) {
        /* Delta encoding. Handle increases and decreases in time, in case the
         * client has adjusted time, or in case of conflicting client times. */
        int64_t utime_delta = utime - last_logged_client_utime;
        uint32_t len = emit_leb128_signed(outbuf, 0, utime_delta);
        len = emit_leb128(outbuf, len, str_len);
        if (str_len) {
            memcpy(outbuf + len, str, str_len);
            len += str_len;
        }
        /* Flag this for high precision time (no truncation) */
        uint32_t new_segment = dbuf_append(last_client_utime_segment,
                                           DBUF_EVENT_TEXT_MESSAGE,
                                           outbuf, len, 0);
        if (new_segment == last_client_utime_segment) {
            /*
             * Commit the values logged. Note this is the only task accessing
             * this state so these updates are synchronized with the last event
             * of this class append.
             */
            last_logged_client_utime = utime;
            break;
        }

        /* Moved on to a new buffer. Reset the delta encoding
         * state and retry. */
        last_client_utime_segment = new_segment;
        last_logged_client_utime = 0;
    }

    free(outbuf);
}


static int handle_message_post(int s, wificfg_method method,
                               uint32_t content_length,
                               wificfg_content_type content_type,
                               char *buf, size_t len)
{
    if (content_type != HTTP_CONTENT_TYPE_WWW_FORM_URLENCODED) {
        return wificfg_write_string(s, "HTTP/1.1 400 \r\n"
                                    "Content-Type: text/html\r\n"
                                    "Content-Length: 0\r\n"
                                    "Connection: close\r\n"
                                    "\r\n");
    }

    size_t rem = content_length;
    bool valp = false;
    uint32_t utimeh = 0, utimel = 0;
    bool utimehp = false, utimelp = false;
    char *str = NULL;
    size_t str_len = 0;

    while (rem > 0) {
        int r = wificfg_form_name_value(s, &valp, &rem, buf, len);

        if (r < 0)
            break;

        wificfg_form_url_decode(buf);

        form_name name = intern_form_name(buf);

        if (valp) {
            int r = wificfg_form_name_value(s, NULL, &rem, buf, len);
            if (r < 0)
                break;

            wificfg_form_url_decode(buf);

            switch (name) {
            case FORM_NAME_MESSAGE: {
                str_len = strlen(buf);
                str = malloc(str_len + 1);
                strcpy(str, buf);
                break;
            }
            case FORM_NAME_UTIMEH: {
                utimeh = strtoul(buf, NULL, 10);
                utimehp = true;
                break;
            }
            case FORM_NAME_UTIMEL: {
                utimel = strtoul(buf, NULL, 10);
                utimelp = true;
                break;
            }
            default:
                break;
            }
        }
    }

    /* Log the event even without a string. */
    if (utimehp && utimelp) {
        log_client_text_message(utimeh, utimel, str, str_len);
    }

    if (str) {
        free(str);
    }

    return wificfg_write_string(s, http_home_redirect);
}


static const char http_success_json_header[] = "HTTP/1.10 200 \r\n"
    "Content-Type: application/json; charset=utf-8\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "Cache-Control: no-store\r\n"
    "Transfer-Encoding: chunked\r\n"
    "Connection: close\r\n"
    "\r\n";

static int handle_recent_data_post(int s, wificfg_method method,
                                   uint32_t content_length,
                                   wificfg_content_type content_type,
                                   char *buf, size_t len)
{
    if (method == HTTP_METHOD_POST) {
        if (content_type != HTTP_CONTENT_TYPE_WWW_FORM_URLENCODED) {
            return wificfg_write_string(s, "HTTP/1.1 400 \r\n"
                                        "Content-Type: text/html\r\n"
                                        "Content-Length: 0\r\n"
                                        "Connection: close\r\n"
                                        "\r\n");
        }

        size_t rem = content_length;
        bool valp = false;
        uint32_t utimeh = 0, utimel = 0;
        bool utimehp = false, utimelp = false;

        while (rem > 0) {
            int r = wificfg_form_name_value(s, &valp, &rem, buf, len);

            if (r < 0)
                break;

            wificfg_form_url_decode(buf);

            form_name name = intern_form_name(buf);

            if (valp) {
                int r = wificfg_form_name_value(s, NULL, &rem, buf, len);
                if (r < 0)
                    break;

                wificfg_form_url_decode(buf);

                switch (name) {
                case FORM_NAME_UTIMEH: {
                    utimeh = strtoul(buf, NULL, 10);
                    utimehp = true;
                    break;
                }
                case FORM_NAME_UTIMEL: {
                    utimel = strtoul(buf, NULL, 10);
                    utimelp = true;
                    break;
                }
                default:
                    break;
                }
            }
        }

        if (utimehp && utimelp) {
            log_client_utime(utimeh, utimel);
        }
    }

    if (wificfg_write_string(s, http_success_json_header) < 0) return -1;

    if (method != HTTP_METHOD_HEAD) {
        uint32_t count = RTC.COUNTER;
        snprintf(buf, len, "{\"counter\":%u", count);
        if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;

        {
            uint32_t counter;
            struct tm time;
            float temp;

            if (ds3231_time_temp(&counter, &time, &temp)) {
                snprintf(buf, len, ",\n \"ds3231_counter\":%u", counter);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                snprintf(buf, len, ", \"ds3231_temp\":%.2f", temp);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
            }
        }

        {
            uint32_t counter;
            float temp, rh;
            if (sht2x_temp_rh(&counter, &temp, &rh)) {
                snprintf(buf, len, ",\n \"sht2x_counter\":%u", counter);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                snprintf(buf, len, ", \"sht2x_temp\":%.1f", temp);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                snprintf(buf, len, ", \"sht2x_rh\":%.1f", rh);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
            }
        }

        {
            uint32_t counter;
            float temp, press;
            if (bmp180_temp_press(&counter, &temp, &press)) {
                snprintf(buf, len, ",\n \"bmp180_counter\":%u", counter);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                snprintf(buf, len, ", \"bmp180_temp\":%.1f", temp);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                snprintf(buf, len, ", \"bmp180_press\":%.0f", press);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
            }
        }

        {
            uint32_t counter;
            float temp, press, rh;
            if (bme280_temp_press_rh(&counter, &temp, &press, &rh)) {
                snprintf(buf, len, ",\n \"bme280_counter\":%u", counter);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                snprintf(buf, len, ", \"bme280_temp\":%.1f", temp);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                snprintf(buf, len, ", \"bme280_press\":%.0f", press);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                snprintf(buf, len, ", \"bme280_rh\":%.1f", rh);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
            }
        }

        {
            uint32_t counter;
            uint16_t pm1a = 0;
            uint16_t pm25a = 0;
            uint16_t pm10a = 0;
            uint16_t pm1b = 0;
            uint16_t pm25b = 0;
            uint16_t pm10b = 0;
            uint16_t c1 = 0;
            uint16_t c2 = 0;
            uint16_t c3 = 0;
            uint16_t c4 = 0;
            uint16_t c5 = 0;
            uint16_t c6 = 0;
            uint16_t r1 = 0;

            if (pms_last_data(&counter, &pm1a, &pm25a, &pm10a, &pm1b, &pm25b, &pm10b, &c1, &c2, &c3, &c4, &c5, &c6, &r1)) {
                snprintf(buf, len, ",\n \"pms_counter\":%u", counter);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                snprintf(buf, len, ", \"pm10a\":%u", pm1a);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                snprintf(buf, len, ", \"pm10b\":%u", pm1b);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                snprintf(buf, len, ", \"pm25a\":%u", pm25a);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                snprintf(buf, len, ", \"pm25b\":%u", pm25b);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                snprintf(buf, len, ", \"pm100a\":%u", pm10a);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                snprintf(buf, len, ", \"pm100b\":%u", pm10b);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;

                snprintf(buf, len, ", \"pc03\":%u", c1);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                snprintf(buf, len, ", \"pc05\":%u", c2);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                snprintf(buf, len, ", \"pc10\":%u", c3);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                snprintf(buf, len, ", \"pc25\":%u", c4);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                snprintf(buf, len, ", \"pc50\":%u", c5);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                snprintf(buf, len, ", \"pc100\":%u", c6);
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
            }
        }

        if (wificfg_write_string_chunk(s, "}\n", buf, len) < 0) return -1;
        if (wificfg_write_chunk_end(s) < 0) return -1;
    }

    return 0;
}


static const char *http_buffer_size_content[] = {
#include "content/bufsize.html"
};

static int handle_buffer_size(int s, wificfg_method method,
                              uint32_t content_length,
                              wificfg_content_type content_type,
                              char *buf, size_t len)
{
    if (wificfg_write_string(s, http_success_header) < 0) return -1;

    if (method != HTTP_METHOD_HEAD) {
        if (wificfg_write_string_chunk(s, http_buffer_size_content[0], buf, len) < 0) return -1;
        if (wificfg_write_chunk_end(s) < 0) return -1;
    }
    return 0;
}


static int handle_buffer_size_post(int s, wificfg_method method,
                                   uint32_t content_length,
                                   wificfg_content_type content_type,
                                   char *buf, size_t len)
{
    if (content_type != HTTP_CONTENT_TYPE_WWW_FORM_URLENCODED) {
        return wificfg_write_string(s, "HTTP/1.1 400 \r\n"
                                    "Content-Type: text/html\r\n"
                                    "Content-Length: 0\r\n"
                                    "Connection: close\r\n"
                                    "\r\n");
    }

    size_t rem = content_length;
    bool valp = false;
    uint32_t utimeh = 0;
    uint32_t utimel = 0;
    uint32_t requested_index = 0xffffffff;

    while (rem > 0) {
        int r = wificfg_form_name_value(s, &valp, &rem, buf, len);

        if (r < 0)
            break;

        wificfg_form_url_decode(buf);

        form_name name = intern_form_name(buf);

        if (valp) {
            int r = wificfg_form_name_value(s, NULL, &rem, buf, len);
            if (r < 0)
                break;

            wificfg_form_url_decode(buf);

            switch (name) {
            case FORM_NAME_UTIMEH: {
                utimeh = strtoul(buf, NULL, 10);
                break;
            }
            case FORM_NAME_UTIMEL: {
                utimel = strtoul(buf, NULL, 10);
                break;
            }
            case FORM_NAME_INDEX: {
                requested_index = strtoul(buf, NULL, 10);
                break;
            }
            default:
                break;
            }
        }
    }

    log_client_utime(utimeh, utimel);

    uint32_t index = 0, next_index = 0xffffffff;
    bool sealed;
    uint32_t size = get_buffer_size(requested_index, &index, &next_index, &sealed);
    if (wificfg_write_string(s, http_success_json_header) < 0) return -1;
    snprintf(buf, len, "{\"index\":%u,\"size\":%u", index, size);
    if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
    snprintf(buf, len, ",\"next\":%u,\"sealed\":%u}", next_index, sealed);
    if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
    if (wificfg_write_chunk_end(s) < 0) return -1;
    return 0;
}

/* A dynamic content-length is appended */
static const char http_success_binary_header[] = "HTTP/1.1 200 \r\n"
    "Content-Type: application/octet-stream\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "Cache-Control: no-store\r\n"
    "Connection: close\r\n";

static int handle_get_buffer_post(int s, wificfg_method method,
                                  uint32_t content_length,
                                  wificfg_content_type content_type,
                                  char *buf, size_t len)
{
    if (content_type != HTTP_CONTENT_TYPE_WWW_FORM_URLENCODED) {
        return wificfg_write_string(s, "HTTP/1.1 400 \r\n"
                                    "Content-Type: text/html\r\n"
                                    "Content-Length: 0\r\n"
                                    "Connection: close\r\n"
                                    "\r\n");
    }

    size_t rem = content_length;
    bool valp = false;
    uint32_t utimeh = 0;
    uint32_t utimel = 0;
    uint32_t requested_index = 0xffffffff;
    uint32_t start = 0;
    uint32_t end = 0;

    while (rem > 0) {
        int r = wificfg_form_name_value(s, &valp, &rem, buf, len);

        if (r < 0)
            break;

        wificfg_form_url_decode(buf);

        form_name name = intern_form_name(buf);

        if (valp) {
            int r = wificfg_form_name_value(s, NULL, &rem, buf, len);
            if (r < 0)
                break;

            wificfg_form_url_decode(buf);

            switch (name) {
            case FORM_NAME_UTIMEH: {
                utimeh = strtoul(buf, NULL, 10);
                break;
            }
            case FORM_NAME_UTIMEL: {
                utimel = strtoul(buf, NULL, 10);
                break;
            }
            case FORM_NAME_INDEX: {
                requested_index = strtoul(buf, NULL, 10);
                break;
            }
            case FORM_NAME_START: {
                start = strtoul(buf, NULL, 10);
                break;
            }
            case FORM_NAME_END: {
                end = strtoul(buf, NULL, 10);
                break;
            }
            default:
                break;
            }
        }
    }

    log_client_utime(utimeh, utimel);

    /*
     * Firstly search for the buffer with the requested index, and
     * note it's current length. Only the current length amount is
     * returned, even if it grows during the request. If the buffer is
     * removed during the request then the response is truncated, and
     * the client is expected to notice such an error.
     */
    uint32_t index = 0, next_index;
    bool sealed;
    uint32_t size = get_buffer_size(requested_index, &index, &next_index, &sealed);

    if (index != requested_index) {
        return wificfg_write_string(s, "HTTP/1.1 404 \r\n"
                                    "Content-Type: text/html\r\n"
                                    "Access-Control-Allow-Origin: *\r\n"
                                    "Content-Length: 0\r\n"
                                    "Connection: close\r\n"
                                    "\r\n");
    }

    if (start >= size)
        start = size;
    if (end >= size)
        end = size;
    if (end < start)
        end = start;

    ssize_t length = end - start;
    if (wificfg_write_string(s, http_success_binary_header) < 0) return -1;
    snprintf(buf, len, "Content-Length: %u\r\n\r\n", length);
    if (wificfg_write_string(s, buf) < 0) return -1;

    while (length > 0) {
        uint32_t chunk = length > len ? len : length;
        if (!get_buffer_range(index, start, start + chunk, (uint8_t *)buf)) return -1;
        if (write(s, buf, chunk) < 0) return -1;
        start += chunk;
        length -= chunk;
    }
    return 0;
}


static const wificfg_dispatch dispatch_list[] = {
    {"/", HTTP_METHOD_GET, handle_index, false},
    {"/index.html", HTTP_METHOD_GET, handle_index, false},
    {"/logging", HTTP_METHOD_POST, handle_logging_post, true},
    {"/logging.html", HTTP_METHOD_POST, handle_logging_post, true},
    {"/config", HTTP_METHOD_GET, handle_config, true},
    {"/config.html", HTTP_METHOD_GET, handle_config, true},
    {"/config", HTTP_METHOD_POST, handle_config_post, true},
    {"/config.html", HTTP_METHOD_POST, handle_config_post, true},
    {"/time", HTTP_METHOD_POST, handle_time_post, true},
    {"/time.html", HTTP_METHOD_POST, handle_time_post, true},
    {"/erasedata.html", HTTP_METHOD_POST, handle_erase_data_post, true},
    {"/message", HTTP_METHOD_POST, handle_message_post, false},
    {"/message.html", HTTP_METHOD_POST, handle_message_post, false},
    //
    {"/smoothie.js", HTTP_METHOD_GET, handle_smoothie, false},
    {"/plot.js", HTTP_METHOD_GET, handle_plot_script, false},
    {"/plot", HTTP_METHOD_GET, handle_plot, false},
    {"/plot.html", HTTP_METHOD_GET, handle_plot, false},
    {"/recentdata", HTTP_METHOD_POST, handle_recent_data_post, false},
    {"/recentdata.html", HTTP_METHOD_POST, handle_recent_data_post, false},
    {"/recentdata", HTTP_METHOD_GET, handle_recent_data_post, false},
    {"/recentdata.html", HTTP_METHOD_GET, handle_recent_data_post, false},
    //
    {"/bufsize", HTTP_METHOD_GET, handle_buffer_size, false},
    {"/bufsize.html", HTTP_METHOD_GET, handle_buffer_size, false},
    {"/bufsize", HTTP_METHOD_POST, handle_buffer_size_post, false},
    {"/bufsize.html", HTTP_METHOD_POST, handle_buffer_size_post, false},
    {"/getbuffer", HTTP_METHOD_POST, handle_get_buffer_post, false},
    {"/getbuffer.html", HTTP_METHOD_POST, handle_get_buffer_post, false},
    {NULL, HTTP_METHOD_ANY, NULL}
};

void init_web()
{
    /* Override the wificfg default AP name and password. */
    wificfg_default_ssid = "OAQ_%02X%02X%02X";
    wificfg_default_password = "oaqwifipw";
    wificfg_default_hostname = "oaq-%02x%02x%02x";

    sdk_wifi_set_sleep_type(WIFI_SLEEP_MODEM);

    /*
     * Always start the Wifi, even if not posting data to a server, to
     * allow local access and configuration.
     */

    wificfg_init(80, dispatch_list);
}
