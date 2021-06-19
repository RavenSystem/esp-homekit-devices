#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "http_buffered_client.h"

#define MAX_REQUEST_SIZE (152 / sizeof(uint32_t))
#define vTaskDelayMs(ms) vTaskDelay((ms) / portTICK_PERIOD_MS)

typedef void (*handle_http_token)(char *);

struct http_token_table {
    const char *      token;
    handle_http_token http_tock_cb;
};

// Response struct
struct HTTP_response {
    unsigned int response_code;
    unsigned int length;
};

/**
 * \addtogroup http_buffer_client_internal
 * Http Request
 */
const char *req =
  "GET %s HTTP/1.1\r\n"
  "Host: %s \r\n"
  "User-Agent: esp-open-rtos/0.1 esp8266\r\n"
  "Connection: close\r\n"
  "\r\n";

static uint32_t request[MAX_REQUEST_SIZE];

static const struct addrinfo hints = {
    .ai_family   = AF_UNSPEC,
    .ai_socktype = SOCK_STREAM,
};

static struct HTTP_response http_reponse;

// HTTP Header Token, add here function and then register it in HTTP Table callback
static void http_handle_cb_ContentLength(char *token)
{
    token += 16; // strlen("Content-Length:"), skip useless part
    while (*token) {
        if (isdigit((int) *token))
            http_reponse.length = (unsigned int) strtol(token, &token, 10);
        else
            token++;
    }
}

static inline void parse_http_header_HTTP_STATUS(char *token)
{
    token += 8; // Skip HTTP/1.0

    while (*token) {
        if (isdigit((int) *token))
            http_reponse.response_code = (unsigned int) strtol(token, &token, 10);
        else
            token++;
    }
}

// HTTP Token Hanling callback
struct http_token_table HTTP_HEADER_TOKEN[] = {
    { .token = "Content-Length", .http_tock_cb = http_handle_cb_ContentLength },
};

static inline void parse_http_header(char *header)
{
    char *str1, *str2, *token, *subtoken, *saveptr1, *saveptr2;
    const char line_split[] = "\r\n", sub_chart[] = ":";
    unsigned int j, i;

    for (j = 1, str1 = header;; j++, str1 = NULL) {
        token = strtok_r(str1, line_split, &saveptr1);
        if (token == NULL)
            break;

        str2     = token;
        subtoken = strtok_r(str2, sub_chart, &saveptr2);
        if (subtoken == NULL)
            break;

        if (j == 1) {
            // Is HTTP Header, response, HTTP Version and status
            parse_http_header_HTTP_STATUS(token);
            continue;
        }

        for (i = 0; i < sizeof(HTTP_HEADER_TOKEN) / sizeof(struct http_token_table); i++)
            if (!strcmp(subtoken, HTTP_HEADER_TOKEN[i].token))
                HTTP_HEADER_TOKEN[i].http_tock_cb(token);

    }
}

HTTP_Client_State HttpClient_dowload(Http_client_info *info)
{
    struct addrinfo *res;
    unsigned int tot_http_pdu_rd, full;
    ssize_t read_byte;
    int err, sock;
    char *wrt_ptr;

    err = getaddrinfo(info->server, info->port, &hints, &res);

    if (err != 0 || res == NULL) {
        if (res)
            freeaddrinfo(res);
        return HTTP_DNS_LOOKUP_FALLIED;
    }

    sock = socket(res->ai_family, res->ai_socktype, 0);
    if (sock < 0) {
        freeaddrinfo(res);
        return HTTP_SOCKET_ALLOCATION_FALLIED;
    }

    if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
        close(sock);
        freeaddrinfo(res);
        return HTTP_SOCKET_CONNECTION_FALLIED;
    }

    // Release address memory
    freeaddrinfo(res);

    // Alloc memory for request
    sprintf((char *) request, req, info->path, info->server);
    if (write(sock, (char *) request, strlen((char *) request)) < 0) {
        close(sock);
        return HTTP_REQUEST_SEND_FALLIED;
    }

    tot_http_pdu_rd = 0;
    wrt_ptr         = info->buffer;
    full = 0;

    // Ping wdog
    vTaskDelayMs(250);
    do {
        int free_buff_space;

        free_buff_space = info->buffer_size - full;
        read_byte       = read(sock, wrt_ptr, free_buff_space);

        // Update buffer property
        wrt_ptr += read_byte;
        full    += read_byte;

        if (tot_http_pdu_rd == 0) {
            // Is fist chunk, then it contains http header, parse it.
            unsigned int header_len, pdu_size;
            char *header, *pdu;

            pdu = strstr(info->buffer, "\r\n\r\n");

            if (pdu != NULL)
                pdu += 4;  // Offset by 4 bytes to start of content
            else           // Not all HTTP Header has been read, then continue read
                continue;

            header_len = pdu - info->buffer + 4;

            header = malloc(header_len + 1); // NULL string terminator

            memset(header, 0, header_len + 1);
            memcpy(header, info->buffer, header_len);

            parse_http_header(header);
            // Release useless memory
            free(header);

            // Move memory
            pdu_size = wrt_ptr - pdu;

            memmove(info->buffer, pdu, pdu_size);
            wrt_ptr = (info->buffer + pdu_size);

            full = pdu_size;
            tot_http_pdu_rd = pdu_size;

            if (http_reponse.response_code != HTTP_OK)
                goto err_label;

            continue;
        }
        tot_http_pdu_rd += read_byte;

        if (full == info->buffer_size) {
            info->buffer_full_cb(info->buffer, full);

            memset(info->buffer, 0, info->buffer_size);
            wrt_ptr = info->buffer;
            full    = 0;
            vTaskDelayMs(50);
        }
    } while (read_byte > 0);

    info->final_cb(info->buffer, full);
    if (tot_http_pdu_rd != http_reponse.length)
        http_reponse.response_code = HTTP_DOWLOAD_SIZE_NOT_MATCH;

err_label:
    close(sock);
    return http_reponse.response_code;
} /* HttpClient_dowload */
