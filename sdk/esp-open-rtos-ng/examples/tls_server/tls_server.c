/* tls_server - simple TLS server that outputs some statistics to a connecting client,
 * then closes the socket.
 *
 * Similar to the the ssl_server example in mbedtls.
 *
 * To test this program, connect to the ESP using openssl command line like this:
 *
 * openssl s_client -connect 192.168.66.209:800
 *
 * The openssl command line client will print some information for the (self-signed) server certificate,
 * then after a couple of seconds (validation) there will be a few lines of text output sent from the ESP.
 *
 * See the cert.c file for private key & certificate (PEM format), plus information for generation.
 *
 * Original Copyright (C) 2006-2015, ARM Limited, All Rights Reserved, Apache 2.0 License.
 * Additions Copyright (C) 2016 Angus Gratton, Apache 2.0 License.
 */
#include "espressif/esp_common.h"
#include "esp/uart.h"

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
extern const char *server_cert;
extern const char *server_key;

/* mbedtls/config.h MUST appear before all other mbedtls headers, or
   you'll get the default config.

   (Although mostly that isn't a big problem, you just might get
   errors at link time if functions don't exist.) */
#include "mbedtls/config.h"

#include "mbedtls/net_sockets.h"
#include "mbedtls/debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/certs.h"

#define PORT "800"

void tls_server_task(void *pvParameters)
{
    int successes = 0, failures = 0, ret;
    printf("TLS server task starting...\n");

    const char *pers = "tls_server";

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_x509_crt srvcert;
    mbedtls_pk_context pkey;
    mbedtls_ssl_config conf;
    mbedtls_net_context server_ctx;

    /*
     * 0. Initialize the RNG and the session data
     */
    mbedtls_ssl_init(&ssl);
    mbedtls_x509_crt_init(&srvcert);
    mbedtls_pk_init( &pkey );
    mbedtls_ctr_drbg_init(&ctr_drbg);
    printf("\n  . Seeding the random number generator...");

    mbedtls_ssl_config_init(&conf);

    mbedtls_entropy_init(&entropy);
    if((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                    (const unsigned char *) pers,
                                    strlen(pers))) != 0)
    {
        printf(" failed\n  ! mbedtls_ctr_drbg_seed returned %d\n", ret);
        abort();
    }

    printf(" ok\n");

    /*
     * 0. Initialize certificates
     */
    printf("  . Loading the server certificate ...");

    ret = mbedtls_x509_crt_parse(&srvcert, (uint8_t*)server_cert, strlen(server_cert)+1);
    if(ret < 0)
    {
        printf(" failed\n  !  mbedtls_x509_crt_parse returned -0x%x\n\n", -ret);
        abort();
    }

    printf(" ok (%d skipped)\n", ret);

    printf("  . Loading the server private key ...");
    ret = mbedtls_pk_parse_key(&pkey, (uint8_t *)server_key, strlen(server_key)+1, NULL, 0);
    if(ret != 0)
    {
        printf(" failed\n ! mbedtls_pk_parse_key returned - 0x%x\n\n", -ret);
        abort();
    }

    printf(" ok\n");

    /*
     * 2. Setup stuff
     */
    printf("  . Setting up the SSL/TLS structure...");

    if((ret = mbedtls_ssl_config_defaults(&conf,
                                          MBEDTLS_SSL_IS_SERVER,
                                          MBEDTLS_SSL_TRANSPORT_STREAM,
                                          MBEDTLS_SSL_PRESET_DEFAULT)) != 0)
    {
        printf(" failed\n  ! mbedtls_ssl_config_defaults returned %d\n\n", ret);
        goto exit;
    }

    printf(" ok\n");

    mbedtls_ssl_conf_ca_chain(&conf, srvcert.next, NULL);
    if( ( ret = mbedtls_ssl_conf_own_cert( &conf, &srvcert, &pkey ) ) != 0 )
    {
        printf( " failed\n  ! mbedtls_ssl_conf_own_cert returned %d\n\n", ret );
        abort();
    }

    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
#ifdef MBEDTLS_DEBUG_C
    mbedtls_debug_set_threshold(DEBUG_LEVEL);
    mbedtls_ssl_conf_dbg(&conf, my_debug, stdout);
#endif

    if((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0)
    {
        printf(" failed\n  ! mbedtls_ssl_setup returned %d\n\n", ret);
        goto exit;
    }

    while(1) {
        mbedtls_net_context client_ctx;
        mbedtls_net_init(&server_ctx);
        mbedtls_net_init(&client_ctx);
        printf("top of loop, free heap = %u\n", xPortGetFreeHeapSize());

        /*
         * 1. Start the connection
         */
        ret = mbedtls_net_bind(&server_ctx, NULL, PORT, MBEDTLS_NET_PROTO_TCP);
        if(ret != 0)
        {
            printf(" failed\n  ! mbedtls_net_bind returned %d\n\n", ret);
            goto exit;
        }

        printf(" ok\n");

        ret=mbedtls_net_accept(&server_ctx, &client_ctx, NULL, 0 , NULL);
        if( ret != 0 ){
            printf(" Failed to accept connection. Restarting.\n");
            mbedtls_net_free(&server_ctx);
            continue;
        }

        mbedtls_ssl_set_bio(&ssl, &client_ctx, mbedtls_net_send, mbedtls_net_recv, NULL);

        /*
         * 4. Handshake
         */
        printf("  . Performing the SSL/TLS handshake...");

        while((ret = mbedtls_ssl_handshake(&ssl)) != 0)
        {
            if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
            {
                printf(" failed\n  ! mbedtls_ssl_handshake returned -0x%x\n\n", -ret);
                goto exit;
            }
        }

        printf(" ok\n");


        /*
         * 3. Write the GET request
         */
        printf("  > Writing status to new client... ");

        struct sockaddr_in peer_addr;
        socklen_t peer_addr_len = sizeof(struct sockaddr_in);
        getpeername(client_ctx.fd, (struct sockaddr *)&peer_addr, &peer_addr_len);
        unsigned char buf[256];
        int len = sprintf((char *) buf, "O hai, client " IPSTR ":%d\nFree heap size is %d bytes\n",
                          IP2STR((ip4_addr_t *)&peer_addr.sin_addr.s_addr),
                          peer_addr.sin_port, xPortGetFreeHeapSize());
        while((ret = mbedtls_ssl_write(&ssl, buf, len)) <= 0)
        {
            if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
            {
                printf(" failed\n  ! mbedtls_ssl_write returned %d\n\n", ret);
                goto exit;
            }
        }

        len = ret;
        ret = 0;
        printf(" %d bytes written. Closing socket on client.\n\n%s", len, (char *) buf);

        mbedtls_ssl_close_notify(&ssl);

    exit:
        mbedtls_ssl_session_reset(&ssl);
        mbedtls_net_free(&client_ctx);
        mbedtls_net_free(&server_ctx);

        if(ret != 0)
        {
            char error_buf[100];
            mbedtls_strerror(ret, error_buf, 100);
            printf("\n\nLast error was: %d - %s\n\n", ret, error_buf);
            failures++;
        } else {
            successes++;
        }

        printf("\n\nsuccesses = %d failures = %d\n", successes, failures);
        printf("Waiting for next client...\n");
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
