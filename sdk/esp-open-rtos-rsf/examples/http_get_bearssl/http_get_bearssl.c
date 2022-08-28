/* http_get_bearssl - HTTPS version of the http_get example, using BearSSL.
 *
 * Retrieves a JSON response from the howsmyssl.com API via HTTPS over TLS v1.2.
 *
 * Validates the server's certificate using a hardcoded public key.
 *
 * Adapted from the client_basic sample in BearSSL.
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

#include "bearssl.h"

#define CLOCK_SECONDS_PER_MINUTE (60UL)
#define CLOCK_MINUTES_PER_HOUR (60UL)
#define CLOCK_HOURS_PER_DAY (24UL)
#define CLOCK_SECONDS_PER_HOUR (CLOCK_MINUTES_PER_HOUR*CLOCK_SECONDS_PER_MINUTE)
#define CLOCK_SECONDS_PER_DAY (CLOCK_HOURS_PER_DAY*CLOCK_SECONDS_PER_HOUR)

#define WEB_SERVER "www.howsmyssl.com"
#define WEB_PORT "443"
#define WEB_URL "https://www.howsmyssl.com/a/check"

#define GET_REQUEST "GET "WEB_URL" HTTP/1.1\nHost: "WEB_SERVER"\n\n"

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
 * The hardcoded trust anchors. These are the two DN + public key that
 * correspond to the self-signed certificates cert-root-rsa.pem and
 * cert-root-ec.pem.
 *
 * C code for hardcoded trust anchors can be generated with the "brssl"
 * command-line tool (with the "ta" command). To build that tool run:
 *
 * $ cd /path/to/esp-open-rtos/extras/bearssl/BearSSL
 * $ make build/brssl
 *
 * Below is the imported "Let's Encrypt" root certificate, as howsmyssl
 * is depending on it:
 *
 * https://letsencrypt.org/certs/letsencryptauthorityx3.pem
 *
 * The generate the trust anchor code below, run:
 *
 * $ /path/to/esp-open-rtos/extras/bearssl/BearSSL/build/brssl \
 *   ta letsencryptauthorityx3.pem
 *
 * To get the server certificate for a given https host:
 *
 * $ openssl s_client -showcerts -servername www.howsmyssl.com \
 *   -connect www.howsmyssl.com:443 < /dev/null | \
 *   openssl x509 -outform pem > server.pem
 */

static const unsigned char TA0_DN[] = {
	0x30, 0x4A, 0x31, 0x0B, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13,
	0x02, 0x55, 0x53, 0x31, 0x16, 0x30, 0x14, 0x06, 0x03, 0x55, 0x04, 0x0A,
	0x13, 0x0D, 0x4C, 0x65, 0x74, 0x27, 0x73, 0x20, 0x45, 0x6E, 0x63, 0x72,
	0x79, 0x70, 0x74, 0x31, 0x23, 0x30, 0x21, 0x06, 0x03, 0x55, 0x04, 0x03,
	0x13, 0x1A, 0x4C, 0x65, 0x74, 0x27, 0x73, 0x20, 0x45, 0x6E, 0x63, 0x72,
	0x79, 0x70, 0x74, 0x20, 0x41, 0x75, 0x74, 0x68, 0x6F, 0x72, 0x69, 0x74,
	0x79, 0x20, 0x58, 0x33
};

static const unsigned char TA0_RSA_N[] = {
	0x9C, 0xD3, 0x0C, 0xF0, 0x5A, 0xE5, 0x2E, 0x47, 0xB7, 0x72, 0x5D, 0x37,
	0x83, 0xB3, 0x68, 0x63, 0x30, 0xEA, 0xD7, 0x35, 0x26, 0x19, 0x25, 0xE1,
	0xBD, 0xBE, 0x35, 0xF1, 0x70, 0x92, 0x2F, 0xB7, 0xB8, 0x4B, 0x41, 0x05,
	0xAB, 0xA9, 0x9E, 0x35, 0x08, 0x58, 0xEC, 0xB1, 0x2A, 0xC4, 0x68, 0x87,
	0x0B, 0xA3, 0xE3, 0x75, 0xE4, 0xE6, 0xF3, 0xA7, 0x62, 0x71, 0xBA, 0x79,
	0x81, 0x60, 0x1F, 0xD7, 0x91, 0x9A, 0x9F, 0xF3, 0xD0, 0x78, 0x67, 0x71,
	0xC8, 0x69, 0x0E, 0x95, 0x91, 0xCF, 0xFE, 0xE6, 0x99, 0xE9, 0x60, 0x3C,
	0x48, 0xCC, 0x7E, 0xCA, 0x4D, 0x77, 0x12, 0x24, 0x9D, 0x47, 0x1B, 0x5A,
	0xEB, 0xB9, 0xEC, 0x1E, 0x37, 0x00, 0x1C, 0x9C, 0xAC, 0x7B, 0xA7, 0x05,
	0xEA, 0xCE, 0x4A, 0xEB, 0xBD, 0x41, 0xE5, 0x36, 0x98, 0xB9, 0xCB, 0xFD,
	0x6D, 0x3C, 0x96, 0x68, 0xDF, 0x23, 0x2A, 0x42, 0x90, 0x0C, 0x86, 0x74,
	0x67, 0xC8, 0x7F, 0xA5, 0x9A, 0xB8, 0x52, 0x61, 0x14, 0x13, 0x3F, 0x65,
	0xE9, 0x82, 0x87, 0xCB, 0xDB, 0xFA, 0x0E, 0x56, 0xF6, 0x86, 0x89, 0xF3,
	0x85, 0x3F, 0x97, 0x86, 0xAF, 0xB0, 0xDC, 0x1A, 0xEF, 0x6B, 0x0D, 0x95,
	0x16, 0x7D, 0xC4, 0x2B, 0xA0, 0x65, 0xB2, 0x99, 0x04, 0x36, 0x75, 0x80,
	0x6B, 0xAC, 0x4A, 0xF3, 0x1B, 0x90, 0x49, 0x78, 0x2F, 0xA2, 0x96, 0x4F,
	0x2A, 0x20, 0x25, 0x29, 0x04, 0xC6, 0x74, 0xC0, 0xD0, 0x31, 0xCD, 0x8F,
	0x31, 0x38, 0x95, 0x16, 0xBA, 0xA8, 0x33, 0xB8, 0x43, 0xF1, 0xB1, 0x1F,
	0xC3, 0x30, 0x7F, 0xA2, 0x79, 0x31, 0x13, 0x3D, 0x2D, 0x36, 0xF8, 0xE3,
	0xFC, 0xF2, 0x33, 0x6A, 0xB9, 0x39, 0x31, 0xC5, 0xAF, 0xC4, 0x8D, 0x0D,
	0x1D, 0x64, 0x16, 0x33, 0xAA, 0xFA, 0x84, 0x29, 0xB6, 0xD4, 0x0B, 0xC0,
	0xD8, 0x7D, 0xC3, 0x93
};

static const unsigned char TA0_RSA_E[] = {
	0x01, 0x00, 0x01
};

static const br_x509_trust_anchor TAs[1] = {
	{
		{ (unsigned char *)TA0_DN, sizeof TA0_DN },
		BR_X509_TA_CA,
		{
			BR_KEYTYPE_RSA,
			{ .rsa = {
				(unsigned char *)TA0_RSA_N, sizeof TA0_RSA_N,
				(unsigned char *)TA0_RSA_E, sizeof TA0_RSA_E,
			} }
		}
	}
};

#define TAs_NUM   1

/*
 * Buffer to store a record + BearSSL state
 * We use MONO mode to save 16k of RAM.
 * This could be even smaller by using max_fragment_len, but
 * the howsmyssl.com server doesn't seem to support it.
 */
static unsigned char bearssl_buffer[BR_SSL_BUFSIZE_MONO];

static br_ssl_client_context sc;
static br_x509_minimal_context xc;
static br_sslio_context ioc;

void http_get_task(void *pvParameters)
{
    int successes = 0, failures = 0;
    int provisional_time = 0;

    while (1) {
        /*
         * Wait until we can resolve the DNS for the server, as an indication
         * our network is probably working...
         */
        const struct addrinfo hints = {
            .ai_family = AF_INET,
            .ai_socktype = SOCK_STREAM,
        };
        struct addrinfo *res = NULL;
        int dns_err = 0;
        do {
            if (res)
                freeaddrinfo(res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            dns_err = getaddrinfo(WEB_SERVER, WEB_PORT, &hints, &res);
        } while(dns_err != 0 || res == NULL);

        int fd = socket(res->ai_family, res->ai_socktype, 0);
        if (fd < 0) {
            freeaddrinfo(res);
            printf("socket failed\n");
            failures++;
            continue;
        }

        printf("Initializing BearSSL... ");
        br_ssl_client_init_full(&sc, &xc, TAs, TAs_NUM);

        /*
         * Set the I/O buffer to the provided array. We allocated a
         * buffer large enough for full-duplex behaviour with all
         * allowed sizes of SSL records, hence we set the last argument
         * to 1 (which means "split the buffer into separate input and
         * output areas").
         */
        br_ssl_engine_set_buffer(&sc.eng, bearssl_buffer, sizeof bearssl_buffer, 0);

        /*
         * Inject some entropy from the ESP hardware RNG
         * This is necessary because we don't support any of the BearSSL methods
         */
        for (int i = 0; i < 10; i++) {
            int rand = hwrand();
            br_ssl_engine_inject_entropy(&sc.eng, &rand, 4);
        }

        /*
         * Reset the client context, for a new handshake. We provide the
         * target host name: it will be used for the SNI extension. The
         * last parameter is 0: we are not trying to resume a session.
         */
        br_ssl_client_reset(&sc, WEB_SERVER, 0);

        /*
         * Initialise the simplified I/O wrapper context, to use our
         * SSL client context, and the two callbacks for socket I/O.
         */
        br_sslio_init(&ioc, &sc.eng, sock_read, &fd, sock_write, &fd);
        printf("done.\r\n");

        /* FIXME: set date & time using epoch time precompiler flag for now */
        provisional_time = CONFIG_EPOCH_TIME + (xTaskGetTickCount()/configTICK_RATE_HZ);
        xc.days = (provisional_time / CLOCK_SECONDS_PER_DAY) + 719528;
        xc.seconds = provisional_time % CLOCK_SECONDS_PER_DAY;
        printf("Time: %02i:%02i\r\n",
            (int)(xc.seconds / CLOCK_SECONDS_PER_HOUR),
            (int)((xc.seconds % CLOCK_SECONDS_PER_HOUR)/CLOCK_SECONDS_PER_MINUTE)
        );

        if (connect(fd, res->ai_addr, res->ai_addrlen) != 0)
        {
            close(fd);
            freeaddrinfo(res);
            printf("connect failed\n");
            failures++;
            continue;
        }
        printf("Connected\r\n");

        /*
         * Note that while the context has, at that point, already
         * assembled the ClientHello to send, nothing happened on the
         * network yet. Real I/O will occur only with the next call.
         *
         * We write our simple HTTP request. We test the call
         * for an error (-1), but this is not strictly necessary, since
         * the error state "sticks": if the context fails for any reason
         * (e.g. bad server certificate), then it will remain in failed
         * state and all subsequent calls will return -1 as well.
         */
        if (br_sslio_write_all(&ioc, GET_REQUEST, strlen(GET_REQUEST)) != BR_ERR_OK) {
            close(fd);
            freeaddrinfo(res);
            printf("br_sslio_write_all failed: %d\r\n", br_ssl_engine_last_error(&sc.eng));
            failures++;
            continue;
        }

        /*
         * SSL is a buffered protocol: we make sure that all our request
         * bytes are sent onto the wire.
         */
        br_sslio_flush(&ioc);

        /*
         * Read and print the server response
         */
        for (;;)
        {
            int rlen;
            unsigned char buf[128];

            bzero(buf, 128);
            // Leave the final byte for zero termination
            rlen = br_sslio_read(&ioc, buf, sizeof(buf) - 1);

            if (rlen < 0) {
                break;
            }
            if (rlen > 0) {
                printf("%s", buf);
            }
        }

        /*
         * If reading the response failed for any reason, we detect it here
         */
        if (br_ssl_engine_last_error(&sc.eng) != BR_ERR_OK) {
            close(fd);
            freeaddrinfo(res);
            printf("failure, error = %d\r\n", br_ssl_engine_last_error(&sc.eng));
            failures++;
            continue;
        }

        printf("\r\n\r\nfree heap pre  = %u\r\n", xPortGetFreeHeapSize());

        /*
         * Close the connection and start over after a delay
         */
        close(fd);
        freeaddrinfo(res);

        printf("free heap post = %u\r\n", xPortGetFreeHeapSize());

        successes++;
        printf("successes = %d failures = %d\r\n", successes, failures);
        for(int countdown = 10; countdown >= 0; countdown--) {
            printf("%d...\n", countdown);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        printf("Starting again!\r\n\r\n");
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

    xTaskCreate(&http_get_task, "get_task", 2048, NULL, 2, NULL);
}
