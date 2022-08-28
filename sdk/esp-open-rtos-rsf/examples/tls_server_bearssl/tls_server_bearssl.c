/* tls_server_bearssl - simple BearSSL TLS server that outputs some statistics to a connecting client,
 * then closes the socket.
 *
 * Uses code from the server_basic example in BearSSL and the tls_server example.
 *
 * To test this program, connect to the ESP using openssl command line like this:
 *
 * openssl s_client -connect 192.168.66.209:800
 *
 * The openssl command line client will print some information for the (self-signed) server certificate,
 * then after a couple of seconds (validation) there will be a few lines of text output sent from the ESP.
 *
 * See the certificate.h and key.h files for private key & certificate, plus information for generation.
 *
 * Original Copyright (c) 2016 Thomas Pornin <pornin@bolet.org>, MIT License.
 * Additions Copyright (C) 2016 Stefan Schake, MIT License.
 */
#include "espressif/esp_common.h"
#include "esp/uart.h"
#include "esp/hwrand.h"

#include <unistd.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "lwip/api.h"

#include "ssid_config.h"

/* Server cert & key */
#include "certificate.h"
#include "key.h"

#include "bearssl.h"

#define PORT 800

/*
 * Low-level data read callback for the simplified SSL I/O API.
 */
static int
sock_read(void *ctx, unsigned char *buf, size_t len)
{
    for (;;) {
        ssize_t rlen;

        rlen = read(*(int *)ctx, buf, len);
        if (rlen <= 0) {
            if (rlen < 0 && errno == EINTR) {
                continue;
            }
            return -1;
        }
        return (int)rlen;
    }
}

/*
 * Low-level data write callback for the simplified SSL I/O API.
 */
static int
sock_write(void *ctx, const unsigned char *buf, size_t len)
{
    for (;;) {
        ssize_t wlen;

        wlen = write(*(int *)ctx, buf, len);
        if (wlen <= 0) {
            if (wlen < 0 && errno == EINTR) {
                continue;
            }
            return -1;
        }
        return (int)wlen;
    }
}

/*
 * BearSSL IO buffer and server state
 */
static unsigned char iobuf[BR_SSL_BUFSIZE_MONO];
br_ssl_server_context sc;
br_sslio_context ioc;

void tls_server_task(void *pvParameters)
{
    int successes = 0, failures = 0;
    printf("TLS server task starting...\n");

    /* Prepare server socket */
    int sfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sfd < 0) {
        printf("Failed to allocate socket.\r\n");
        return;
    }
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_port = htons(PORT);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sfd, (const struct sockaddr*) &addr, sizeof(addr)) < 0) {
        close(sfd);
        printf("Failed to bind socket\r\n");
        return;
    }
    if (listen(sfd, 0) < 0) {
        close(sfd);
        printf("Failed to listen\r\n");
        return;
    }

    /* Initialize engine */
    br_ssl_server_init_full_rsa(&sc, SERVER_CERTIFICATE_CHAIN, SERVER_CERTIFICATE_CHAIN_LEN, &SERVER_PRIVATE_KEY);
    br_ssl_engine_set_buffer(&sc.eng, iobuf, sizeof iobuf, 0);

    /*
     * Inject some entropy from the ESP hardware RNG
     * This is necessary because we don't support any of the BearSSL methods
     */
    for (int i = 0; i < 10; i++) {
        int rand = hwrand();
        br_ssl_engine_inject_entropy(&sc.eng, &rand, 4);
    }

    while(1) {
        printf("Top of the loop, free heap = %u\r\n", xPortGetFreeHeapSize());

        /* Accept a new client */
        struct sockaddr_in sa;
        socklen_t sa_len = sizeof(sa);
        int cfd = accept(sfd, (struct sockaddr*)&sa, &sa_len);
        if (cfd < 0) {
            continue;
        }

        /* Prepare for a new handshake */
        br_ssl_server_reset(&sc);
        /* Initialize the simplified IO wrapper */
        br_sslio_init(&ioc, &sc.eng, sock_read, &cfd, sock_write, &cfd);

        /* Prepare a message to the client */
        unsigned char buf[100];
        int len = sprintf((char *) buf, "O hai, client " IPSTR ":%d\r\nFree heap size is %d bytes\r\n",
                          IP2STR((ip4_addr_t *)&sa.sin_addr.s_addr),
                          ntohs(sa.sin_port), xPortGetFreeHeapSize());

        /* Send the message and close the connection */
        br_sslio_write_all(&ioc, buf, len);
        br_sslio_close(&ioc);

        /* Check if something bad happened */
        if (br_ssl_engine_last_error(&sc.eng) != BR_ERR_OK) {
            close(cfd);
            printf("failure, error = %d\r\n", br_ssl_engine_last_error(&sc.eng));
            failures++;
            continue;
        }

        close(cfd);
        successes++;
        printf("successes = %d failures = %d\r\n", successes, failures);
        printf("Waiting for next client...\r\n");
    }
}

void user_init(void)
{
    uart_set_baud(0, 115200);
    printf("SDK version:%s\n", sdk_system_get_sdk_version());

    struct sdk_station_config config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASS,
    };

    /* required to call wifi_set_opmode before station_set_config */
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&config);

    xTaskCreate(&tls_server_task, "server_task", 2048, NULL, 2, NULL);
}
