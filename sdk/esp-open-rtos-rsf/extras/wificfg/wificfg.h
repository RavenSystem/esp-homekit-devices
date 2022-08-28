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

#ifndef __WIFICFG_H__
#define __WIFICFG_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Printf format used to initialize a default AP ssid. It is passed the last
 * three bytes of the mac address. This may be NULL to not default the ssid,
 * but the AP network will not run without a ssid.
 */
extern const char *wificfg_default_ssid;

/*
 * A default password for the AP interface. This may be NULL to not default the
 * password, but the AP network will not run without a password. The minimum
 * length is 8 characters.
 */
extern const char *wificfg_default_password;

/*
 * A default hostname printf format string. This may be NULL to not default the
 * hostname.
 */
extern const char *wificfg_default_hostname;

/*
 * The web server parses the http method string in these enums. The ANY method
 * is only use for dispatch. The method enum is passed to the handler functions.
 */
typedef enum {
    HTTP_METHOD_GET,
    HTTP_METHOD_POST,
    HTTP_METHOD_HEAD,
    HTTP_METHOD_OTHER,
    HTTP_METHOD_ANY,
} wificfg_method;

/*
 * The web server parses these content-type header values. This is passed to the
 * dispatch function.
 */
typedef enum {
    HTTP_CONTENT_TYPE_WWW_FORM_URLENCODED,
    HTTP_CONTENT_TYPE_OTHER
} wificfg_content_type;

/*
 * The function signature for the http server request handler functions.
 *
 * The buffer, with its length, is usable by the handler.
 */
typedef int (* wificfg_handler)(int s, wificfg_method method,
                                uint32_t content_length,
                                wificfg_content_type content_type,
                                char *buf, size_t len);

typedef struct {
    const char *path;
    wificfg_method method;
    wificfg_handler handler;
    bool secure;
} wificfg_dispatch;


/*
 * Start the Wifi Configuration http server task. The IP port number
 * and a path dispatch list are needed. The dispatch list can not be
 * stack allocated as it is passed to another task.
 */
void wificfg_init(uint32_t port, const wificfg_dispatch *dispatch);

/*
 * Support for reading a form name or value from the socket. The name or value
 * is truncated to the buffer length. The number of characters read is limited
 * to the remainder which is updated. The 'valp' flag is set if a value follows.
 */
ssize_t wificfg_form_name_value(int s, bool *valp, size_t *rem, char *buf, size_t len);

/* Support for form url-encoding decoder. */
void wificfg_form_url_decode(char *string);

/* Support for html-escaping of form values. */
void wificfg_html_escape(char *string, char *buf, size_t len);

/* Support for writing a string in a response. */
ssize_t wificfg_write_string(int s, const char *str);

/* Support for writing a string in a response, with chunk transfer encoding.
 * An optional buffer may be supplied to use to construct a chunk with the
 * header and trailer, reducing the number of write() calls, and the str may be
 * at the start of this buffer.
 */
ssize_t wificfg_write_string_chunk(int s, const char *str, char *buf, size_t len);

/* Write a chunk transfer encoding end marker. */
ssize_t wificfg_write_chunk_end(int s);

/* Write a html title meta data, using the hostname or AP SSI. */
int wificfg_write_html_title(int s, char *buf, size_t len, const char *str);

/* Wait until the station interface has connected to an access point,
 * and obtained an IP address. */
void wificfg_wait_until_sta_connected(void);

bool wificfg_add_shutdown_hook(void (*fn)(void *), void *arg);

#ifdef __cplusplus
}
#endif

#endif  // __WIFICFG_H__
