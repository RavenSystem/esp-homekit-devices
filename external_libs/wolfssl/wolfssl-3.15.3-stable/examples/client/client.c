/* client.c
 *
 * Copyright (C) 2006-2017 wolfSSL Inc.
 *
 * This file is part of wolfSSL.
 *
 * wolfSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * wolfSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */


#ifdef HAVE_CONFIG_H
        #include <config.h>
#endif

#include <wolfssl/wolfcrypt/settings.h>

#include <wolfssl/ssl.h>

#if defined(WOLFSSL_MDK_ARM) || defined(WOLFSSL_KEIL_TCP_NET)
        #include <stdio.h>
        #include <string.h>
        #include "cmsis_os.h"
        #include "rl_fs.h"
        #include "rl_net.h"
        #include "wolfssl_MDK_ARM.h"
#endif

#include <wolfssl/test.h>

#include <examples/client/client.h>
#include <wolfssl/error-ssl.h>

#ifndef NO_WOLFSSL_CLIENT

#ifdef WOLFSSL_ASYNC_CRYPT
    static int devId = INVALID_DEVID;
#endif

#define DEFAULT_TIMEOUT_SEC 2

/* Note on using port 0: the client standalone example doesn't utilize the
 * port 0 port sharing; that is used by (1) the server in external control
 * test mode and (2) the testsuite which uses this code and sets up the correct
 * port numbers when the internal thread using the server code using port 0. */


#ifdef WOLFSSL_CALLBACKS
    Timeval timeout;
    static int handShakeCB(HandShakeInfo* info)
    {
        (void)info;
        return 0;
    }

    static int timeoutCB(TimeoutInfo* info)
    {
        (void)info;
        return 0;
    }

#endif

#ifdef HAVE_SESSION_TICKET
    static int sessionTicketCB(WOLFSSL* ssl,
                        const unsigned char* ticket, int ticketSz,
                        void* ctx)
    {
        (void)ssl;
        (void)ticket;
        printf("Session Ticket CB: ticketSz = %d, ctx = %s\n",
               ticketSz, (char*)ctx);
        return 0;
    }
#endif

static int NonBlockingSSL_Connect(WOLFSSL* ssl)
{
    int ret;
    int error;
    SOCKET_T sockfd;
    int select_ret = 0;

#ifndef WOLFSSL_CALLBACKS
    ret = wolfSSL_connect(ssl);
#else
    ret = wolfSSL_connect_ex(ssl, handShakeCB, timeoutCB, timeout);
#endif
    error = wolfSSL_get_error(ssl, 0);
    sockfd = (SOCKET_T)wolfSSL_get_fd(ssl);

    while (ret != WOLFSSL_SUCCESS &&
        (error == WOLFSSL_ERROR_WANT_READ || error == WOLFSSL_ERROR_WANT_WRITE
        #ifdef WOLFSSL_ASYNC_CRYPT
            || error == WC_PENDING_E
        #endif
        #ifdef WOLFSSL_NONBLOCK_OCSP
            || error == OCSP_WANT_READ
        #endif
    )) {
        int currTimeout = 1;

        if (error == WOLFSSL_ERROR_WANT_READ)
            printf("... client would read block\n");
        else if (error == WOLFSSL_ERROR_WANT_WRITE)
            printf("... client would write block\n");

#ifdef WOLFSSL_ASYNC_CRYPT
        if (error == WC_PENDING_E) {
            ret = wolfSSL_AsyncPoll(ssl, WOLF_POLL_FLAG_CHECK_HW);
            if (ret < 0) break;
        }
        else
#endif
        {
    #ifdef WOLFSSL_DTLS
            currTimeout = wolfSSL_dtls_get_current_timeout(ssl);
    #endif
            select_ret = tcp_select(sockfd, currTimeout);
        }

        if ((select_ret == TEST_RECV_READY) || (select_ret == TEST_ERROR_READY)
        #ifdef WOLFSSL_ASYNC_CRYPT
            || error == WC_PENDING_E
        #endif
        ) {
        #ifndef WOLFSSL_CALLBACKS
            ret = wolfSSL_connect(ssl);
        #else
            ret = wolfSSL_connect_ex(ssl, handShakeCB, timeoutCB, timeout);
        #endif
            error = wolfSSL_get_error(ssl, 0);
        }
        else if (select_ret == TEST_TIMEOUT && !wolfSSL_dtls(ssl)) {
            error = WOLFSSL_ERROR_WANT_READ;
        }
#ifdef WOLFSSL_DTLS
        else if (select_ret == TEST_TIMEOUT && wolfSSL_dtls(ssl) &&
                                        wolfSSL_dtls_got_timeout(ssl) >= 0) {
            error = WOLFSSL_ERROR_WANT_READ;
        }
#endif
        else {
            error = WOLFSSL_FATAL_ERROR;
        }
    }

    return ret;
}


static void ShowCiphers(void)
{
    char ciphers[4096];

    int ret = wolfSSL_get_ciphers(ciphers, (int)sizeof(ciphers));

    if (ret == WOLFSSL_SUCCESS)
        printf("%s\n", ciphers);
}

/* Shows which versions are valid */
static void ShowVersions(void)
{
#ifndef NO_OLD_TLS
    #ifdef WOLFSSL_ALLOW_SSLV3
        printf("0:");
    #endif
    #ifdef WOLFSSL_ALLOW_TLSV10
        printf("1:");
    #endif
    printf("2:");
#endif /* NO_OLD_TLS */
#ifndef WOLFSSL_NO_TLS12
    printf("3:");
#endif
#ifdef WOLFSSL_TLS13
    printf("4:");
#endif
    printf("\n");
}

#ifdef WOLFSSL_TLS13
static void SetKeyShare(WOLFSSL* ssl, int onlyKeyShare, int useX25519)
{
    int groups[3];
    int count = 0;

    (void)useX25519;

    WOLFSSL_START(WC_FUNC_CLIENT_KEY_EXCHANGE_SEND);
    if (onlyKeyShare == 0 || onlyKeyShare == 2) {
    #ifdef HAVE_CURVE25519
        if (useX25519) {
            groups[count++] = WOLFSSL_ECC_X25519;
            if (wolfSSL_UseKeyShare(ssl, WOLFSSL_ECC_X25519) != WOLFSSL_SUCCESS)
                err_sys("unable to use curve x25519");
        }
        else
    #endif
        {
    #ifdef HAVE_ECC
        #if defined(HAVE_ECC256) || defined(HAVE_ALL_CURVES)
            groups[count++] = WOLFSSL_ECC_SECP256R1;
            if (wolfSSL_UseKeyShare(ssl, WOLFSSL_ECC_SECP256R1)
                                                           != WOLFSSL_SUCCESS) {
                err_sys("unable to use curve secp256r1");
            }
        #endif
    #endif
        }
    }
    if (onlyKeyShare == 0 || onlyKeyShare == 1) {
    #ifdef HAVE_FFDHE_2048
        groups[count++] = WOLFSSL_FFDHE_2048;
        if (wolfSSL_UseKeyShare(ssl, WOLFSSL_FFDHE_2048) != WOLFSSL_SUCCESS)
            err_sys("unable to use DH 2048-bit parameters");
    #endif
    }

    if (wolfSSL_set_groups(ssl, groups, count) != WOLFSSL_SUCCESS)
        err_sys("unable to set groups");
    WOLFSSL_END(WC_FUNC_CLIENT_KEY_EXCHANGE_SEND);
}
#endif

/* Measures average time to create, connect and disconnect a connection (TPS).
Benchmark = number of connections. */
static int ClientBenchmarkConnections(WOLFSSL_CTX* ctx, char* host, word16 port,
    int dtlsUDP, int dtlsSCTP, int benchmark, int resumeSession, int useX25519,
    int helloRetry, int onlyKeyShare, int version)
{
    /* time passed in number of connects give average */
    int times = benchmark, skip = times * 0.1;
    int loops = resumeSession ? 2 : 1;
    int i = 0, err, ret;
#ifndef NO_SESSION_CACHE
    WOLFSSL_SESSION* benchSession = NULL;
#endif
#ifdef WOLFSSL_TLS13
    byte* reply[80];
    static const char msg[] = "GET /index.html HTTP/1.0\r\n\r\n";
#endif

    (void)resumeSession;
    (void)useX25519;
    (void)helloRetry;
    (void)onlyKeyShare;
    (void)version;

    while (loops--) {
    #ifndef NO_SESSION_CACHE
        int benchResume = resumeSession && loops == 0;
    #endif
        double start = current_time(1), avg;

        for (i = 0; i < times; i++) {
            SOCKET_T sockfd;
            WOLFSSL* ssl;

            if (i == skip)
                start = current_time(1);

            ssl = wolfSSL_new(ctx);
            if (ssl == NULL)
                err_sys("unable to get SSL object");

        #ifndef NO_SESSION_CACHE
            if (benchResume)
                wolfSSL_set_session(ssl, benchSession);
        #endif
        #ifdef WOLFSSL_TLS13
            else if (version >= 4) {
                if (!helloRetry)
                    SetKeyShare(ssl, onlyKeyShare, useX25519);
                else
                    wolfSSL_NoKeyShares(ssl);
            }
        #endif

            tcp_connect(&sockfd, host, port, dtlsUDP, dtlsSCTP, ssl);

            if (wolfSSL_set_fd(ssl, sockfd) != WOLFSSL_SUCCESS) {
                err_sys("error in setting fd");
            }

            do {
                err = 0; /* reset error */
                ret = wolfSSL_connect(ssl);
                if (ret != WOLFSSL_SUCCESS) {
                    err = wolfSSL_get_error(ssl, 0);
                #ifdef WOLFSSL_ASYNC_CRYPT
                    if (err == WC_PENDING_E) {
                        ret = wolfSSL_AsyncPoll(ssl, WOLF_POLL_FLAG_CHECK_HW);
                        if (ret < 0) break;
                    }
                #endif
                }
            } while (err == WC_PENDING_E);
            if (ret != WOLFSSL_SUCCESS) {
                err_sys("SSL_connect failed");
            }

    #ifdef WOLFSSL_TLS13
            if (version >= 4 && resumeSession && !benchResume) {
                if (wolfSSL_write(ssl, msg, sizeof(msg)-1) <= 0)
                    err_sys("SSL_write failed");

                if (wolfSSL_read(ssl, reply, sizeof(reply)-1) <= 0)
                    err_sys("SSL_read failed");
            }
    #endif


            wolfSSL_shutdown(ssl);
    #ifndef NO_SESSION_CACHE
            if (i == (times-1) && resumeSession) {
                benchSession = wolfSSL_get_session(ssl);
            }
    #endif
            wolfSSL_free(ssl);
            CloseSocket(sockfd);
        }
        avg = current_time(0) - start;
        avg /= (times - skip);
        avg *= 1000;   /* milliseconds */
    #ifndef NO_SESSION_CACHE
        if (benchResume)
            printf("wolfSSL_resume  avg took: %8.3f milliseconds\n", avg);
        else
    #endif
            printf("wolfSSL_connect avg took: %8.3f milliseconds\n", avg);

        WOLFSSL_TIME(times);
    }

    return EXIT_SUCCESS;
}

/* Measures throughput in kbps. Throughput = number of bytes */
static int ClientBenchmarkThroughput(WOLFSSL_CTX* ctx, char* host, word16 port,
    int dtlsUDP, int dtlsSCTP, int block, int throughput, int useX25519)
{
    double start, conn_time = 0, tx_time = 0, rx_time = 0;
    SOCKET_T sockfd;
    WOLFSSL* ssl;
    int ret = 0, err = 0;

    start = current_time(1);
    ssl = wolfSSL_new(ctx);
    if (ssl == NULL)
        err_sys("unable to get SSL object");

    tcp_connect(&sockfd, host, port, dtlsUDP, dtlsSCTP, ssl);
    if (wolfSSL_set_fd(ssl, sockfd) != WOLFSSL_SUCCESS) {
        err_sys("error in setting fd");
    }

    (void)useX25519;
    #ifdef WOLFSSL_TLS13
        #ifdef HAVE_CURVE25519
            if (useX25519) {
                if (wolfSSL_UseKeyShare(ssl, WOLFSSL_ECC_X25519)
                        != WOLFSSL_SUCCESS) {
                    err_sys("unable to use curve x25519");
                }
            }
        #endif
    #endif

    do {
        err = 0; /* reset error */
        ret = wolfSSL_connect(ssl);
        if (ret != WOLFSSL_SUCCESS) {
            err = wolfSSL_get_error(ssl, 0);
        #ifdef WOLFSSL_ASYNC_CRYPT
            if (err == WC_PENDING_E) {
                ret = wolfSSL_AsyncPoll(ssl, WOLF_POLL_FLAG_CHECK_HW);
                if (ret < 0) break;
            }
        #endif
        }
    } while (err == WC_PENDING_E);
    if (ret == WOLFSSL_SUCCESS) {
        /* Perform throughput test */
        char *tx_buffer, *rx_buffer;

        /* Record connection time */
        conn_time = current_time(0) - start;

        /* Allocate TX/RX buffers */
        tx_buffer = (char*)XMALLOC(block, NULL, DYNAMIC_TYPE_TMP_BUFFER);
        rx_buffer = (char*)XMALLOC(block, NULL, DYNAMIC_TYPE_TMP_BUFFER);
        if (tx_buffer && rx_buffer) {
            WC_RNG rng;

            /* Startup the RNG */
        #if !defined(HAVE_FIPS) && defined(WOLFSSL_ASYNC_CRYPT)
            ret = wc_InitRng_ex(&rng, NULL, devId);
        #else
            ret = wc_InitRng(&rng);
        #endif
            if (ret == 0) {
                int xfer_bytes;

                /* Generate random data to send */
                ret = wc_RNG_GenerateBlock(&rng, (byte*)tx_buffer, block);
                wc_FreeRng(&rng);
                if(ret != 0) {
                    err_sys("wc_RNG_GenerateBlock failed");
                }

                /* Perform TX and RX of bytes */
                xfer_bytes = 0;
                while (throughput > xfer_bytes) {
                    int len, rx_pos, select_ret;

                    /* Determine packet size */
                    len = min(block, throughput - xfer_bytes);

                    /* Perform TX */
                    start = current_time(1);
                    do {
                        err = 0; /* reset error */
                        ret = wolfSSL_write(ssl, tx_buffer, len);
                        if (ret <= 0) {
                            err = wolfSSL_get_error(ssl, 0);
                        #ifdef WOLFSSL_ASYNC_CRYPT
                            if (err == WC_PENDING_E) {
                                ret = wolfSSL_AsyncPoll(ssl, WOLF_POLL_FLAG_CHECK_HW);
                                if (ret < 0) break;
                            }
                        #endif
                        }
                    } while (err == WC_PENDING_E);
                    if (ret != len) {
                        printf("SSL_write bench error %d!\n", err);
                        err_sys("SSL_write failed");
                    }
                    tx_time += current_time(0) - start;

                    /* Perform RX */
                    select_ret = tcp_select(sockfd, DEFAULT_TIMEOUT_SEC);
                    if (select_ret == TEST_RECV_READY) {
                        start = current_time(1);
                        rx_pos = 0;
                        while (rx_pos < len) {
                            ret = wolfSSL_read(ssl, &rx_buffer[rx_pos],
                                                                len - rx_pos);
                            if (ret <= 0) {
                                err = wolfSSL_get_error(ssl, 0);
                            #ifdef WOLFSSL_ASYNC_CRYPT
                                if (err == WC_PENDING_E) {
                                    ret = wolfSSL_AsyncPoll(ssl, WOLF_POLL_FLAG_CHECK_HW);
                                    if (ret < 0) break;
                                }
                                else
                            #endif
                                if (err != WOLFSSL_ERROR_WANT_READ) {
                                    printf("SSL_read bench error %d\n", err);
                                    err_sys("SSL_read failed");
                                }
                            }
                            else {
                                rx_pos += ret;
                            }
                        }
                        rx_time += current_time(0) - start;
                    }

                    /* Compare TX and RX buffers */
                    if(XMEMCMP(tx_buffer, rx_buffer, len) != 0) {
                        free(tx_buffer);
                        tx_buffer = NULL;
                        free(rx_buffer);
                        rx_buffer = NULL;
                        err_sys("Compare TX and RX buffers failed");
                    }

                    /* Update overall position */
                    xfer_bytes += len;
                }
            }
            else {
                err_sys("wc_InitRng failed");
            }
        }
        else {
            err_sys("Client buffer malloc failed");
        }
        if(tx_buffer) XFREE(tx_buffer, NULL, DYNAMIC_TYPE_TMP_BUFFER);
        if(rx_buffer) XFREE(rx_buffer, NULL, DYNAMIC_TYPE_TMP_BUFFER);
    }
    else {
        err_sys("wolfSSL_connect failed");
    }

    wolfSSL_shutdown(ssl);
    wolfSSL_free(ssl);
    CloseSocket(sockfd);

    printf("wolfSSL Client Benchmark %d bytes\n"
        "\tConnect %8.3f ms\n"
        "\tTX      %8.3f ms (%8.3f MBps)\n"
        "\tRX      %8.3f ms (%8.3f MBps)\n",
        throughput,
        conn_time * 1000,
        tx_time * 1000, throughput / tx_time / 1024 / 1024,
        rx_time * 1000, throughput / rx_time / 1024 / 1024
    );

    return EXIT_SUCCESS;
}

const char* starttlsCmd[6] = {
    "220",
    "EHLO mail.example.com\r\n",
    "250",
    "STARTTLS\r\n",
    "220",
    "QUIT\r\n",
};

/* Initiates the STARTTLS command sequence over TCP */
static int StartTLS_Init(SOCKET_T* sockfd)
{
    char tmpBuf[256];

    if (sockfd == NULL)
        return BAD_FUNC_ARG;

    /* S: 220 <host> SMTP service ready */
    XMEMSET(tmpBuf, 0, sizeof(tmpBuf));
    if (recv(*sockfd, tmpBuf, sizeof(tmpBuf)-1, 0) < 0)
        err_sys("failed to read STARTTLS command\n");

    if (!XSTRNCMP(tmpBuf, starttlsCmd[0], XSTRLEN(starttlsCmd[0]))) {
        printf("%s\n", tmpBuf);
    } else {
        err_sys("incorrect STARTTLS command received");
    }

    /* C: EHLO mail.example.com */
    if (send(*sockfd, starttlsCmd[1], (int)XSTRLEN(starttlsCmd[1]), 0) !=
              (int)XSTRLEN(starttlsCmd[1]))
        err_sys("failed to send STARTTLS EHLO command\n");

    /* S: 250 <host> offers a warm hug of welcome */
    XMEMSET(tmpBuf, 0, sizeof(tmpBuf));
    if (recv(*sockfd, tmpBuf, sizeof(tmpBuf)-1, 0) < 0)
        err_sys("failed to read STARTTLS command\n");

    if (!XSTRNCMP(tmpBuf, starttlsCmd[2], XSTRLEN(starttlsCmd[2]))) {
        printf("%s\n", tmpBuf);
    } else {
        err_sys("incorrect STARTTLS command received");
    }

    /* C: STARTTLS */
    if (send(*sockfd, starttlsCmd[3], (int)XSTRLEN(starttlsCmd[3]), 0) !=
              (int)XSTRLEN(starttlsCmd[3])) {
        err_sys("failed to send STARTTLS command\n");
    }

    /* S: 220 Go ahead */
    XMEMSET(tmpBuf, 0, sizeof(tmpBuf));
    if (recv(*sockfd, tmpBuf, sizeof(tmpBuf)-1, 0) < 0)
        err_sys("failed to read STARTTLS command\n");

    if (!XSTRNCMP(tmpBuf, starttlsCmd[4], XSTRLEN(starttlsCmd[4]))) {
        printf("%s\n", tmpBuf);
    } else {
        err_sys("incorrect STARTTLS command received, expected 220");
    }

    return WOLFSSL_SUCCESS;
}

/* Closes down the SMTP connection */
static int SMTP_Shutdown(WOLFSSL* ssl, int wc_shutdown)
{
    int ret, err = 0;
    char tmpBuf[256];

    if (ssl == NULL)
        return BAD_FUNC_ARG;

    printf("\nwolfSSL client shutting down SMTP connection\n");

    XMEMSET(tmpBuf, 0, sizeof(tmpBuf));

    /* C: QUIT */
    do {
        ret = wolfSSL_write(ssl, starttlsCmd[5], (int)XSTRLEN(starttlsCmd[5]));
        if (ret < 0) {
            err = wolfSSL_get_error(ssl, 0);
        #ifdef WOLFSSL_ASYNC_CRYPT
            if (err == WC_PENDING_E) {
                ret = wolfSSL_AsyncPoll(ssl, WOLF_POLL_FLAG_CHECK_HW);
                if (ret < 0) break;
            }
        #endif
        }
    } while (err == WC_PENDING_E);
    if (ret != (int)XSTRLEN(starttlsCmd[5])) {
        err_sys("failed to send SMTP QUIT command\n");
    }

    /* S: 221 2.0.0 Service closing transmission channel */
    do {
        ret = wolfSSL_read(ssl, tmpBuf, sizeof(tmpBuf));
        if (ret < 0) {
            err = wolfSSL_get_error(ssl, 0);
        #ifdef WOLFSSL_ASYNC_CRYPT
            if (err == WC_PENDING_E) {
                ret = wolfSSL_AsyncPoll(ssl, WOLF_POLL_FLAG_CHECK_HW);
                if (ret < 0) break;
            }
        #endif
        }
    } while (err == WC_PENDING_E);
    if (ret < 0) {
        err_sys("failed to read SMTP closing down response\n");
    }

    printf("%s\n", tmpBuf);

    ret = wolfSSL_shutdown(ssl);
    if (wc_shutdown && ret == WOLFSSL_SHUTDOWN_NOT_DONE)
        wolfSSL_shutdown(ssl);    /* bidirectional shutdown */

    return WOLFSSL_SUCCESS;
}

static void ClientWrite(WOLFSSL* ssl, char* msg, int msgSz)
{
    int ret, err;
    char buffer[WOLFSSL_MAX_ERROR_SZ];

    do {
        err = 0; /* reset error */
        ret = wolfSSL_write(ssl, msg, msgSz);
        if (ret <= 0) {
            err = wolfSSL_get_error(ssl, 0);
        #ifdef WOLFSSL_ASYNC_CRYPT
            if (err == WC_PENDING_E) {
                ret = wolfSSL_AsyncPoll(ssl, WOLF_POLL_FLAG_CHECK_HW);
                if (ret < 0) break;
            }
        #endif
        }
    } while (err == WOLFSSL_ERROR_WANT_WRITE
    #ifdef WOLFSSL_ASYNC_CRYPT
        || err == WC_PENDING_E
    #endif
    );
    if (ret != msgSz) {
        printf("SSL_write msg error %d, %s\n", err,
                                        wolfSSL_ERR_error_string(err, buffer));
        err_sys("SSL_write failed");
    }
}

static void ClientRead(WOLFSSL* ssl, char* reply, int replyLen, int mustRead)
{
    int ret, err;
    char buffer[WOLFSSL_MAX_ERROR_SZ];

    do {
        err = 0; /* reset error */
        ret = wolfSSL_read(ssl, reply, replyLen);
        if (ret <= 0) {
            err = wolfSSL_get_error(ssl, 0);
        #ifdef WOLFSSL_ASYNC_CRYPT
            if (err == WC_PENDING_E) {
                ret = wolfSSL_AsyncPoll(ssl, WOLF_POLL_FLAG_CHECK_HW);
                if (ret < 0) break;
            }
            else
        #endif
            if (err != WOLFSSL_ERROR_WANT_READ) {
                printf("SSL_read reply error %d, %s\n", err,
                                         wolfSSL_ERR_error_string(err, buffer));
                err_sys("SSL_read failed");
            }
        }
    } while ((mustRead && err == WOLFSSL_ERROR_WANT_READ)
    #ifdef WOLFSSL_ASYNC_CRYPT
        || err == WC_PENDING_E
    #endif
    );
    if (ret > 0) {
        reply[ret] = 0;
        printf("%s\n", reply);
    }
}

static void Usage(void)
{
    printf("client "    LIBWOLFSSL_VERSION_STRING
           " NOTE: All files relative to wolfSSL home dir\n");
    printf("-?          Help, print this usage\n");
    printf("-h <host>   Host to connect to, default %s\n", wolfSSLIP);
    printf("-p <num>    Port to connect on, not 0, default %d\n", wolfSSLPort);
#ifndef WOLFSSL_TLS13
    printf("-v <num>    SSL version [0-3], SSLv3(0) - TLS1.2(3)), default %d\n",
                                 CLIENT_DEFAULT_VERSION);
    printf("-V          Prints valid ssl version numbers, SSLv3(0) - TLS1.2(3)\n");
#else
    printf("-v <num>    SSL version [0-4], SSLv3(0) - TLS1.3(4)), default %d\n",
                                 CLIENT_DEFAULT_VERSION);
    printf("-V          Prints valid ssl version numbers, SSLv3(0) - TLS1.3(4)\n");
#endif
    printf("-l <str>    Cipher suite list (: delimited)\n");
    printf("-c <file>   Certificate file,           default %s\n", cliCertFile);
    printf("-k <file>   Key file,                   default %s\n", cliKeyFile);
    printf("-A <file>   Certificate Authority file, default %s\n", caCertFile);
#ifndef NO_DH
    printf("-Z <num>    Minimum DH key bits,        default %d\n",
                                 DEFAULT_MIN_DHKEY_BITS);
#endif
    printf("-b <num>    Benchmark <num> connections and print stats\n");
#ifdef HAVE_ALPN
    printf("-L <str>    Application-Layer Protocol Negotiation ({C,F}:<list>)\n");
#endif
    printf("-B <num>    Benchmark throughput using <num> bytes and print stats\n");
    printf("-s          Use pre Shared keys\n");
    printf("-d          Disable peer checks\n");
    printf("-D          Override Date Errors example\n");
    printf("-e          List Every cipher suite available, \n");
    printf("-g          Send server HTTP GET\n");
    printf("-u          Use UDP DTLS,"
           " add -v 2 for DTLSv1, -v 3 for DTLSv1.2 (default)\n");
#ifdef WOLFSSL_SCTP
    printf("-G          Use SCTP DTLS,"
           " add -v 2 for DTLSv1, -v 3 for DTLSv1.2 (default)\n");
#endif
    printf("-m          Match domain name in cert\n");
    printf("-N          Use Non-blocking sockets\n");
#ifndef NO_SESSION_CACHE
    printf("-r          Resume session\n");
#endif
    printf("-w          Wait for bidirectional shutdown\n");
    printf("-M <prot>   Use STARTTLS, using <prot> protocol (smtp)\n");
#ifdef HAVE_SECURE_RENEGOTIATION
    printf("-R          Allow Secure Renegotiation\n");
    printf("-i          Force client Initiated Secure Renegotiation\n");
#endif
    printf("-f          Fewer packets/group messages\n");
    printf("-x          Disable client cert/key loading\n");
    printf("-X          Driven by eXternal test case\n");
    printf("-j          Use verify callback override\n");
#ifdef SHOW_SIZES
    printf("-z          Print structure sizes\n");
#endif
#ifdef HAVE_SNI
    printf("-S <str>    Use Host Name Indication\n");
#endif
#ifdef HAVE_MAX_FRAGMENT
    printf("-F <num>    Use Maximum Fragment Length [1-5]\n");
#endif
#ifdef HAVE_TRUNCATED_HMAC
    printf("-T          Use Truncated HMAC\n");
#endif
#ifdef HAVE_EXTENDED_MASTER
    printf("-n          Disable Extended Master Secret\n");
#endif
#ifdef HAVE_OCSP
    printf("-o          Perform OCSP lookup on peer certificate\n");
    printf("-O <url>    Perform OCSP lookup using <url> as responder\n");
#endif
#if defined(HAVE_CERTIFICATE_STATUS_REQUEST) \
 || defined(HAVE_CERTIFICATE_STATUS_REQUEST_V2)
    printf("-W          Use OCSP Stapling\n");
#endif
#ifdef ATOMIC_USER
    printf("-U          Atomic User Record Layer Callbacks\n");
#endif
#ifdef HAVE_PK_CALLBACKS
    printf("-P          Public Key Callbacks\n");
#endif
#ifdef HAVE_ANON
    printf("-a          Anonymous client\n");
#endif
#ifdef HAVE_CRL
    printf("-C          Disable CRL\n");
#endif
#ifdef WOLFSSL_TRUST_PEER_CERT
    printf("-E <file>   Path to load trusted peer cert\n");
#endif
#ifdef HAVE_WNR
    printf("-q <file>   Whitewood config file,      default %s\n", wnrConfig);
#endif
    printf("-H <arg>    Internal tests [defCipherList, exitWithRet]\n");
#ifdef WOLFSSL_TLS13
    printf("-J          Use HelloRetryRequest to choose group for KE\n");
    printf("-K          Key Exchange for PSK not using (EC)DHE\n");
    printf("-I          Update keys and IVs before sending data\n");
#ifndef NO_DH
    printf("-y          Key Share with FFDHE named groups only\n");
#endif
#ifdef HAVE_ECC
    printf("-Y          Key Share with ECC named groups only\n");
#endif
#endif /* WOLFSSL_TLS13 */
#ifdef HAVE_CURVE25519
    printf("-t          Use X25519 for key exchange\n");
#endif
#if defined(WOLFSSL_TLS13) && defined(WOLFSSL_POST_HANDSHAKE_AUTH)
    printf("-Q          Support requesting certificate post-handshake\n");
#endif
#ifdef WOLFSSL_EARLY_DATA
    printf("-0          Early data sent to server (0-RTT handshake)\n");
#endif
#ifdef WOLFSSL_MULTICAST
    printf("-3 <grpid>  Multicast, grpid < 256\n");
#endif
}

THREAD_RETURN WOLFSSL_THREAD client_test(void* args)
{
    SOCKET_T sockfd = WOLFSSL_SOCKET_INVALID;

    wolfSSL_method_func method = NULL;
    WOLFSSL_CTX*     ctx     = 0;
    WOLFSSL*         ssl     = 0;

    WOLFSSL*         sslResume = 0;
    WOLFSSL_SESSION* session = 0;

#ifndef WOLFSSL_ALT_TEST_STRINGS
    char msg[32] = "hello wolfssl!";   /* GET may make bigger */
    char resumeMsg[32] = "resuming wolfssl!";
#else
    char msg[32] = "hello wolfssl!\n";
    char resumeMsg[32] = "resuming wolfssl!\n";
#endif

    char reply[80];
    int  msgSz = (int)XSTRLEN(msg);
    int  resumeSz = (int)XSTRLEN(resumeMsg);

    word16 port   = wolfSSLPort;
    char* host   = (char*)wolfSSLIP;
    const char* domain = "localhost";  /* can't default to www.wolfssl.com
                                          because can't tell if we're really
                                          going there to detect old chacha-poly
                                       */
    int    ch;
    int    version = CLIENT_INVALID_VERSION;
    int    usePsk   = 0;
    int    useAnon  = 0;
    int    sendGET  = 0;
    int    benchmark = 0;
    int    block = TEST_BUFFER_SIZE;
    int    throughput = 0;
    int    doDTLS    = 0;
    int    dtlsUDP   = 0;
    int    dtlsSCTP  = 0;
    int    doMcast   = 0;
    int    matchName = 0;
    int    doPeerCheck = 1;
    int    nonBlocking = 0;
    int    resumeSession = 0;
    int    wc_shutdown   = 0;
    int    disableCRL    = 0;
    int    externalTest  = 0;
    int    ret;
    int    err           = 0;
    int    scr           = 0;    /* allow secure renegotiation */
    int    forceScr      = 0;    /* force client initiaed scr */
#ifndef WOLFSSL_NO_CLIENT_AUTH
    int    useClientCert = 1;
#else
    int    useClientCert = 0;
#endif
    int    fewerPackets  = 0;
    int    atomicUser    = 0;
#ifdef HAVE_PK_CALLBACKS
    int    pkCallbacks   = 0;
    PkCbInfo pkCbInfo;
#endif
    int    overrideDateErrors = 0;
    int    minDhKeyBits  = DEFAULT_MIN_DHKEY_BITS;
    char*  alpnList = NULL;
    unsigned char alpn_opt = 0;
    char*  cipherList = NULL;
    int    useDefCipherList = 0;
    const char* verifyCert = caCertFile;
    const char* ourCert    = cliCertFile;
    const char* ourKey     = cliKeyFile;

    int   doSTARTTLS    = 0;
    char* starttlsProt = NULL;
    int   useVerifyCb = 0;

#ifdef WOLFSSL_TRUST_PEER_CERT
    const char* trustCert  = NULL;
#endif

#ifdef HAVE_SNI
    char*  sniHostName = NULL;
#endif
#ifdef HAVE_MAX_FRAGMENT
    byte maxFragment = 0;
#endif
#ifdef HAVE_TRUNCATED_HMAC
    byte truncatedHMAC = 0;
#endif
#if defined(HAVE_CERTIFICATE_STATUS_REQUEST) \
 || defined(HAVE_CERTIFICATE_STATUS_REQUEST_V2)
    byte statusRequest = 0;
#endif
#ifdef HAVE_EXTENDED_MASTER
    byte disableExtMasterSecret = 0;
#endif
    int helloRetry = 0;
    int onlyKeyShare = 0;
#ifdef WOLFSSL_TLS13
    int noPskDheKe = 0;
    int postHandAuth = 0;
#endif
    int updateKeysIVs = 0;
#ifdef WOLFSSL_EARLY_DATA
    int earlyData = 0;
#endif
#ifdef WOLFSSL_MULTICAST
    byte mcastID = 0;
#endif

#ifdef HAVE_OCSP
    int    useOcsp  = 0;
    char*  ocspUrl  = NULL;
#endif
    int useX25519 = 0;
    int exitWithRet = 0;

#ifdef HAVE_WNR
    const char* wnrConfigFile = wnrConfig;
#endif
    char buffer[WOLFSSL_MAX_ERROR_SZ];

    int     argc = ((func_args*)args)->argc;
    char**  argv = ((func_args*)args)->argv;


#ifdef WOLFSSL_STATIC_MEMORY
    #if (defined(HAVE_ECC) && !defined(ALT_ECC_SIZE)) \
        || defined(SESSION_CERTS)
        /* big enough to handle most cases including session certs */
        byte memory[320000];
    #else
        byte memory[80000];
    #endif
    byte memoryIO[34500]; /* max for IO buffer (TLS packet can be 16k) */
    WOLFSSL_MEM_CONN_STATS ssl_stats;
    #ifdef DEBUG_WOLFSSL
        WOLFSSL_MEM_STATS mem_stats;
    #endif
#endif

    ((func_args*)args)->return_code = -1; /* error state */

#ifdef NO_RSA
    verifyCert = (char*)caEccCertFile;
    ourCert    = (char*)cliEccCertFile;
    ourKey     = (char*)cliEccKeyFile;
#endif
    (void)resumeSz;
    (void)session;
    (void)sslResume;
    (void)atomicUser;
    (void)scr;
    (void)forceScr;
    (void)ourKey;
    (void)ourCert;
    (void)verifyCert;
    (void)useClientCert;
    (void)overrideDateErrors;
    (void)disableCRL;
    (void)minDhKeyBits;
    (void)alpnList;
    (void)alpn_opt;
    (void)updateKeysIVs;
    (void)useX25519;
    (void)helloRetry;
    (void)onlyKeyShare;

    StackTrap();

#ifndef WOLFSSL_VXWORKS
    /* Not used: All used */
    while ((ch = mygetopt(argc, argv, "?"
            "ab:c:defgh:ijk:l:mnop:q:rstuv:wxyz"
            "A:B:CDE:F:GH:IJKL:M:NO:PQRS:TUVW:XYZ:"
            "03:")) != -1) {
        switch (ch) {
            case '?' :
                Usage();
                exit(EXIT_SUCCESS);

            case 'g' :
                sendGET = 1;
                break;

            case 'd' :
                doPeerCheck = 0;
                break;

            case 'e' :
                ShowCiphers();
                exit(EXIT_SUCCESS);

            case 'D' :
                overrideDateErrors = 1;
                break;

            case 'C' :
                #ifdef HAVE_CRL
                    disableCRL = 1;
                #endif
                break;

            case 'u' :
                doDTLS = 1;
                dtlsUDP = 1;
                break;

            case 'G' :
            #ifdef WOLFSSL_SCTP
                doDTLS = 1;
                dtlsSCTP = 1;
            #endif
                break;

            case 's' :
                usePsk = 1;
                break;

            #ifdef WOLFSSL_TRUST_PEER_CERT
            case 'E' :
                trustCert = myoptarg;
                break;
            #endif

            case 'm' :
                matchName = 1;
                break;

            case 'x' :
                useClientCert = 0;
                break;

            case 'X' :
                externalTest = 1;
                break;

            case 'f' :
                fewerPackets = 1;
                break;

            case 'U' :
            #ifdef ATOMIC_USER
                atomicUser = 1;
            #endif
                break;

            case 'P' :
            #ifdef HAVE_PK_CALLBACKS
                pkCallbacks = 1;
            #endif
                break;

            case 'h' :
                host   = myoptarg;
                domain = myoptarg;
                break;

            case 'p' :
                port = (word16)atoi(myoptarg);
                #if !defined(NO_MAIN_DRIVER) || defined(USE_WINDOWS_API)
                    if (port == 0)
                        err_sys("port number cannot be 0");
                #endif
                break;

            case 'v' :
                if (myoptarg[0] == 'd') {
                    version = CLIENT_DOWNGRADE_VERSION;
                    break;
                }
                version = atoi(myoptarg);
                if (version < 0 || version > 4) {
                    Usage();
                    exit(MY_EX_USAGE);
                }
                break;

            case 'V' :
                ShowVersions();
                exit(EXIT_SUCCESS);

            case 'l' :
                cipherList = myoptarg;
                break;

            case 'H' :
                if (XSTRNCMP(myoptarg, "defCipherList", 13) == 0) {
                    printf("Using default cipher list for testing\n");
                    useDefCipherList = 1;
                }
                else if (XSTRNCMP(myoptarg, "exitWithRet", 7) == 0) {
                    printf("Skip exit() for testing\n");
                    exitWithRet = 1;
                }
                else {
                    Usage();
                    exit(MY_EX_USAGE);
                }
                break;

            case 'A' :
                verifyCert = myoptarg;
                break;

            case 'c' :
                ourCert = myoptarg;
                break;

            case 'k' :
                ourKey = myoptarg;
                break;

            case 'Z' :
                #ifndef NO_DH
                    minDhKeyBits = atoi(myoptarg);
                    if (minDhKeyBits <= 0 || minDhKeyBits > 16000) {
                        Usage();
                        exit(MY_EX_USAGE);
                    }
                #endif
                break;

            case 'b' :
                benchmark = atoi(myoptarg);
                if (benchmark < 0 || benchmark > 1000000) {
                    Usage();
                    exit(MY_EX_USAGE);
                }
                break;

            case 'B' :
                throughput = atoi(myoptarg);
                for (; *myoptarg != '\0'; myoptarg++) {
                    if (*myoptarg == ',') {
                        block = atoi(myoptarg + 1);
                        break;
                    }
                }
                if (throughput <= 0 || block <= 0) {
                    Usage();
                    exit(MY_EX_USAGE);
                }
                break;

            case 'N' :
                nonBlocking = 1;
                break;

            case 'r' :
                resumeSession = 1;
                break;

            case 'w' :
                wc_shutdown = 1;
                break;

            case 'R' :
                #ifdef HAVE_SECURE_RENEGOTIATION
                    scr = 1;
                #endif
                break;

            case 'i' :
                #ifdef HAVE_SECURE_RENEGOTIATION
                    scr      = 1;
                    forceScr = 1;
                #endif
                break;

            case 'z' :
                #ifndef WOLFSSL_LEANPSK
                    wolfSSL_GetObjectSize();
                #endif
                break;

            case 'S' :
                #ifdef HAVE_SNI
                    sniHostName = myoptarg;
                #endif
                break;

            case 'F' :
                #ifdef HAVE_MAX_FRAGMENT
                    maxFragment = atoi(myoptarg);
                    if (maxFragment < WOLFSSL_MFL_2_9 ||
                                               maxFragment > WOLFSSL_MFL_2_13) {
                        Usage();
                        exit(MY_EX_USAGE);
                    }
                #endif
                break;

            case 'T' :
                #ifdef HAVE_TRUNCATED_HMAC
                    truncatedHMAC = 1;
                #endif
                break;

            case 'n' :
                #ifdef HAVE_EXTENDED_MASTER
                    disableExtMasterSecret = 1;
                #endif
                break;

            case 'W' :
                #if defined(HAVE_CERTIFICATE_STATUS_REQUEST) \
                 || defined(HAVE_CERTIFICATE_STATUS_REQUEST_V2)
                    statusRequest = atoi(myoptarg);
                #endif
                break;

            case 'o' :
                #ifdef HAVE_OCSP
                    useOcsp = 1;
                #endif
                break;

            case 'O' :
                #ifdef HAVE_OCSP
                    useOcsp = 1;
                    ocspUrl = myoptarg;
                #endif
                break;

            case 'a' :
                #ifdef HAVE_ANON
                    useAnon = 1;
                #endif
                break;

            case 'L' :
                #ifdef HAVE_ALPN
                    alpnList = myoptarg;

                    if (alpnList[0] == 'C' && alpnList[1] == ':')
                        alpn_opt = WOLFSSL_ALPN_CONTINUE_ON_MISMATCH;
                    else if (alpnList[0] == 'F' && alpnList[1] == ':')
                        alpn_opt = WOLFSSL_ALPN_FAILED_ON_MISMATCH;
                    else {
                        Usage();
                        exit(MY_EX_USAGE);
                    }

                    alpnList += 2;

                #endif
                break;

            case 'M' :
                doSTARTTLS = 1;
                starttlsProt = myoptarg;

                if (XSTRNCMP(starttlsProt, "smtp", 4) != 0) {
                    Usage();
                    exit(MY_EX_USAGE);
                }

                break;

            case 'q' :
                #ifdef HAVE_WNR
                    wnrConfigFile = myoptarg;
                #endif
                break;

            case 'J' :
                #ifdef WOLFSSL_TLS13
                    helloRetry = 1;
                #endif
                break;

            case 'K' :
                #ifdef WOLFSSL_TLS13
                    noPskDheKe = 1;
                #endif
                break;

            case 'I' :
                #ifdef WOLFSSL_TLS13
                    updateKeysIVs = 1;
                #endif
                break;

            case 'y' :
                #if defined(WOLFSSL_TLS13) && !defined(NO_DH)
                    onlyKeyShare = 1;
                #endif
                break;

            case 'Y' :
                #if defined(WOLFSSL_TLS13) && defined(HAVE_ECC)
                    onlyKeyShare = 2;
                #endif
                break;

            case 'j' :
                useVerifyCb = 1;
                break;

            case 't' :
                #ifdef HAVE_CURVE25519
                    useX25519 = 1;
                    #if defined(WOLFSSL_TLS13) && defined(HAVE_ECC)
                        onlyKeyShare = 2;
                    #endif
                #endif
                break;

            case 'Q' :
                #if defined(WOLFSSL_TLS13) && \
                                            defined(WOLFSSL_POST_HANDSHAKE_AUTH)
                    postHandAuth = 1;
                #endif
                break;

            case '0' :
            #ifdef WOLFSSL_EARLY_DATA
                earlyData = 1;
            #endif
                break;

            case '3' :
                #ifdef WOLFSSL_MULTICAST
                    doMcast = 1;
                    mcastID = (byte)(atoi(myoptarg) & 0xFF);
                #endif
                break;

            default:
                Usage();
                exit(MY_EX_USAGE);
        }
    }

    myoptind = 0;      /* reset for test cases */
#endif /* !WOLFSSL_VXWORKS */

    if (externalTest) {
        /* detect build cases that wouldn't allow test against wolfssl.com */
        int done = 0;

        #ifdef NO_RSA
            done += 1;
        #endif

        /* www.globalsign.com does not respond to ipv6 ocsp requests */
        #if defined(TEST_IPV6) && defined(HAVE_OCSP)
            done += 1;
        #endif

        /* www.globalsign.com has limited supported cipher suites */
        #if defined(NO_AES) && defined(HAVE_OCSP)
            done += 1;
        #endif

        /* www.globalsign.com only supports static RSA or ECDHE with AES */
        /* We cannot expect users to have on static RSA so test for ECC only
         * as some users will most likely be on 32-bit systems where ECC
         * is not enabled by default */
        #if defined(HAVE_OCSP) && !defined(HAVE_ECC)
            done += 1;
        #endif

        #ifndef NO_PSK
            done += 1;
        #endif

        #ifdef NO_SHA
            done += 1;  /* external cert chain most likely has SHA */
        #endif

        #if !defined(HAVE_ECC) && !defined(WOLFSSL_STATIC_RSA) \
            || ( defined(HAVE_ECC) && !defined(HAVE_SUPPORTED_CURVES) \
                  && !defined(WOLFSSL_STATIC_RSA) )
            /* google needs ECDHE+Supported Curves or static RSA */
            if (!XSTRNCMP(domain, "www.google.com", 14))
                done += 1;
        #endif

        #if !defined(HAVE_ECC) && !defined(WOLFSSL_STATIC_RSA)
            /* wolfssl needs ECDHE or static RSA */
            if (!XSTRNCMP(domain, "www.wolfssl.com", 15))
                done += 1;
        #endif

        #if !defined(WOLFSSL_SHA384)
            if (!XSTRNCMP(domain, "www.wolfssl.com", 15)) {
                /* wolfssl need sha384 for cert chain verify */
                done += 1;
            }
        #endif

        #if !defined(HAVE_AESGCM) && defined(NO_AES) && \
            !(defined(HAVE_CHACHA) && defined(HAVE_POLY1305))
            /* need at least on of these for external tests */
            done += 1;
        #endif

        #if defined(HAVE_QSH)
            /*currently google server rejects client hello with QSH extension.*/
            done += 1;
        #endif

        /* For the external test, if we disable AES, GoDaddy will reject the
         * connection. They only currently support AES suites, RC4 and 3DES
         * suites. With AES disabled we only offer PolyChacha suites. */
        #if defined(NO_AES) && !defined(HAVE_AESGCM)
            if (!XSTRNCMP(domain, "www.wolfssl.com", 15)) {
                done += 1;
            }
        #endif

        if (done) {
            printf("external test can't be run in this mode");

            ((func_args*)args)->return_code = 0;
            exit(EXIT_SUCCESS);
        }
    }

    /* sort out DTLS versus TLS versions */
    if (version == CLIENT_INVALID_VERSION) {
        if (doDTLS)
            version = CLIENT_DTLS_DEFAULT_VERSION;
        else
            version = CLIENT_DEFAULT_VERSION;
    }
    else {
        if (doDTLS) {
            if (version == 3)
                version = -2;
            else
                version = -1;
        }
    }

#ifdef HAVE_WNR
    if (wc_InitNetRandom(wnrConfigFile, NULL, 5000) != 0)
        err_sys("can't load whitewood net random config file");
#endif

    switch (version) {
#ifndef NO_OLD_TLS
    #ifdef WOLFSSL_ALLOW_SSLV3
        case 0:
            method = wolfSSLv3_client_method_ex;
            break;
    #endif

    #ifndef NO_TLS
        #ifdef WOLFSSL_ALLOW_TLSV10
        case 1:
            method = wolfTLSv1_client_method_ex;
            break;
        #endif

        case 2:
            method = wolfTLSv1_1_client_method_ex;
            break;
    #endif /* !NO_TLS */
#endif /* !NO_OLD_TLS */

#ifndef NO_TLS
    #ifndef WOLFSSL_NO_TLS12
        case 3:
            method = wolfTLSv1_2_client_method_ex;
            break;
    #endif

    #ifdef WOLFSSL_TLS13
        case 4:
            method = wolfTLSv1_3_client_method_ex;
            break;
    #endif

        case CLIENT_DOWNGRADE_VERSION:
            method = wolfSSLv23_client_method_ex;
            break;
#endif /* NO_TLS */

#ifdef WOLFSSL_DTLS
        #ifndef NO_OLD_TLS
        case -1:
            method = wolfDTLSv1_client_method_ex;
            break;
        #endif

    #ifndef WOLFSSL_NO_TLS12
        case -2:
            method = wolfDTLSv1_2_client_method_ex;
            break;
    #endif
#endif

        default:
            err_sys("Bad SSL version");
            break;
    }

    if (method == NULL)
        err_sys("unable to get method");


#ifdef WOLFSSL_STATIC_MEMORY
    #ifdef DEBUG_WOLFSSL
    /* print off helper buffer sizes for use with static memory
     * printing to stderr incase of debug mode turned on */
    fprintf(stderr, "static memory management size = %d\n",
            wolfSSL_MemoryPaddingSz());
    fprintf(stderr, "calculated optimum general buffer size = %d\n",
            wolfSSL_StaticBufferSz(memory, sizeof(memory), 0));
    fprintf(stderr, "calculated optimum IO buffer size      = %d\n",
            wolfSSL_StaticBufferSz(memoryIO, sizeof(memoryIO),
                                                  WOLFMEM_IO_POOL_FIXED));
    #endif /* DEBUG_WOLFSSL */

    if (wolfSSL_CTX_load_static_memory(&ctx, method, memory, sizeof(memory),
                                                     0, 1) != WOLFSSL_SUCCESS) {
        err_sys("unable to load static memory");
    }

    if (wolfSSL_CTX_load_static_memory(&ctx, NULL, memoryIO, sizeof(memoryIO),
           WOLFMEM_IO_POOL_FIXED | WOLFMEM_TRACK_STATS, 1) != WOLFSSL_SUCCESS) {
        err_sys("unable to load static memory");
    }
#else
    ctx = wolfSSL_CTX_new(method(NULL));
#endif
    if (ctx == NULL)
        err_sys("unable to get ctx");

#ifdef SINGLE_THREADED
    if (wolfSSL_CTX_new_rng(ctx) != WOLFSSL_SUCCESS) {
        wolfSSL_CTX_free(ctx);
        err_sys("Single Threaded new rng at CTX failed");
    }
#endif

    if (cipherList && !useDefCipherList) {
        if (wolfSSL_CTX_set_cipher_list(ctx, cipherList) != WOLFSSL_SUCCESS) {
            wolfSSL_CTX_free(ctx);
            err_sys("client can't set cipher list 1");
        }
    }

#ifdef WOLFSSL_LEANPSK
    if (!usePsk) {
        usePsk = 1;
    }
#endif

#if defined(NO_RSA) && !defined(HAVE_ECC)
    if (!usePsk) {
        usePsk = 1;
    }
#endif

    if (fewerPackets)
        wolfSSL_CTX_set_group_messages(ctx);

#ifndef NO_DH
    wolfSSL_CTX_SetMinDhKey_Sz(ctx, (word16)minDhKeyBits);
#endif

    if (usePsk) {
#ifndef NO_PSK
        wolfSSL_CTX_set_psk_client_callback(ctx, my_psk_client_cb);
        if (cipherList == NULL) {
            const char *defaultCipherList;
        #if defined(HAVE_AESGCM) && !defined(NO_DH)
            #ifdef WOLFSSL_TLS13
                defaultCipherList = "DHE-PSK-AES128-GCM-SHA256:"
                                    "TLS13-AES128-GCM-SHA256";
            #else
                defaultCipherList = "DHE-PSK-AES128-GCM-SHA256";
            #endif
        #elif defined(HAVE_NULL_CIPHER)
                defaultCipherList = "PSK-NULL-SHA256";
        #else
                defaultCipherList = "PSK-AES128-CBC-SHA256";
        #endif
            if (wolfSSL_CTX_set_cipher_list(ctx,defaultCipherList)
                                                            !=WOLFSSL_SUCCESS) {
                wolfSSL_CTX_free(ctx);
                err_sys("client can't set cipher list 2");
            }
        }
#endif
        if (useClientCert) {
            useClientCert = 0;
        }
    }

    if (useAnon) {
#ifdef HAVE_ANON
        if (cipherList == NULL || (cipherList && useDefCipherList)) {
            const char* defaultCipherList;
            wolfSSL_CTX_allow_anon_cipher(ctx);
            defaultCipherList = "ADH-AES256-GCM-SHA384:"
                                "ADH-AES128-SHA";
            if (wolfSSL_CTX_set_cipher_list(ctx,defaultCipherList)
                                                           != WOLFSSL_SUCCESS) {
                wolfSSL_CTX_free(ctx);
                err_sys("client can't set cipher list 4");
            }
        }
#endif
        if (useClientCert) {
            useClientCert = 0;
        }
    }

#ifdef WOLFSSL_SCTP
    if (dtlsSCTP)
        wolfSSL_CTX_dtls_set_sctp(ctx);
#endif

#ifdef WOLFSSL_ENCRYPTED_KEYS
    wolfSSL_CTX_set_default_passwd_cb(ctx, PasswordCallBack);
#endif

#if defined(WOLFSSL_SNIFFER)
    if (cipherList == NULL) {
        /* don't use EDH, can't sniff tmp keys */
        if (wolfSSL_CTX_set_cipher_list(ctx, "AES128-SHA") != WOLFSSL_SUCCESS) {
            wolfSSL_CTX_free(ctx);
            err_sys("client can't set cipher list 3");
        }
    }
#endif

#ifdef HAVE_OCSP
    if (useOcsp) {
    #ifdef HAVE_IO_TIMEOUT
        wolfIO_SetTimeout(DEFAULT_TIMEOUT_SEC);
    #endif

        if (ocspUrl != NULL) {
            wolfSSL_CTX_SetOCSP_OverrideURL(ctx, ocspUrl);
            wolfSSL_CTX_EnableOCSP(ctx, WOLFSSL_OCSP_NO_NONCE
                                                    | WOLFSSL_OCSP_URL_OVERRIDE);
        }
        else {
            wolfSSL_CTX_EnableOCSP(ctx, WOLFSSL_OCSP_CHECKALL);
        }

    #ifdef WOLFSSL_NONBLOCK_OCSP
        wolfSSL_CTX_SetOCSP_Cb(ctx, OCSPIOCb, OCSPRespFreeCb, NULL);
    #endif
    }
#endif

#ifdef USER_CA_CB
    wolfSSL_CTX_SetCACb(ctx, CaCb);
#endif

#ifdef HAVE_EXT_CACHE
    wolfSSL_CTX_sess_set_get_cb(ctx, mySessGetCb);
    wolfSSL_CTX_sess_set_new_cb(ctx, mySessNewCb);
    wolfSSL_CTX_sess_set_remove_cb(ctx, mySessRemCb);
#endif

#ifndef NO_CERTS
    if (useClientCert){
    #ifndef NO_FILESYSTEM
        if (wolfSSL_CTX_use_certificate_chain_file(ctx, ourCert)
                                                           != WOLFSSL_SUCCESS) {
            wolfSSL_CTX_free(ctx);
            err_sys("can't load client cert file, check file and run from"
                    " wolfSSL home dir");
        }
    #else
        load_buffer(ctx, ourCert, WOLFSSL_CERT_CHAIN);
    #endif

    #ifdef HAVE_PK_CALLBACKS
        pkCbInfo.ourKey = ourKey;
        #ifdef TEST_PK_PRIVKEY
        if (!pkCallbacks)
        #endif
    #endif
    #ifndef NO_FILESYSTEM
        if (wolfSSL_CTX_use_PrivateKey_file(ctx, ourKey, WOLFSSL_FILETYPE_PEM)
                                         != WOLFSSL_SUCCESS) {
            wolfSSL_CTX_free(ctx);
            err_sys("can't load client private key file, check file and run "
                    "from wolfSSL home dir");
        }
    #else
        load_buffer(ctx, ourKey, WOLFSSL_KEY);
    #endif
    }

    if (!usePsk && !useAnon && !useVerifyCb) {
    #if !defined(NO_FILESYSTEM)
        if (wolfSSL_CTX_load_verify_locations(ctx, verifyCert,0)
                                                           != WOLFSSL_SUCCESS) {
            wolfSSL_CTX_free(ctx);
            err_sys("can't load ca file, Please run from wolfSSL home dir");
        }
    #else
        load_buffer(ctx, verifyCert, WOLFSSL_CA);
    #endif  /* !NO_FILESYSTEM */
    #ifdef HAVE_ECC
        /* load ecc verify too, echoserver uses it by default w/ ecc */
        #ifndef NO_FILESYSTEM
        if (wolfSSL_CTX_load_verify_locations(ctx, eccCertFile, 0)
                                                           != WOLFSSL_SUCCESS) {
            wolfSSL_CTX_free(ctx);
            err_sys("can't load ecc ca file, Please run from wolfSSL home dir");
        }
        #else
        load_buffer(ctx, eccCertFile, WOLFSSL_CA);
        #endif /* !NO_FILESYSTEM */
    #endif /* HAVE_ECC */
    #if defined(WOLFSSL_TRUST_PEER_CERT) && !defined(NO_FILESYSTEM)
        if (trustCert) {
            if ((ret = wolfSSL_CTX_trust_peer_cert(ctx, trustCert,
                                    WOLFSSL_FILETYPE_PEM)) != WOLFSSL_SUCCESS) {
                wolfSSL_CTX_free(ctx);
                err_sys("can't load trusted peer cert file");
            }
        }
    #endif /* WOLFSSL_TRUST_PEER_CERT && !NO_FILESYSTEM */
    }
    if (useVerifyCb)
        wolfSSL_CTX_set_verify(ctx, WOLFSSL_VERIFY_PEER, myVerify);
    else if (!usePsk && !useAnon && doPeerCheck == 0)
        wolfSSL_CTX_set_verify(ctx, WOLFSSL_VERIFY_NONE, 0);
    else if (!usePsk && !useAnon && overrideDateErrors == 1)
        wolfSSL_CTX_set_verify(ctx, WOLFSSL_VERIFY_PEER, myDateCb);
#endif /* !NO_CERTS */

#ifdef WOLFSSL_ASYNC_CRYPT
    ret = wolfAsync_DevOpen(&devId);
    if (ret < 0) {
        printf("Async device open failed\nRunning without async\n");
    }
    wolfSSL_CTX_UseAsync(ctx, devId);
#endif /* WOLFSSL_ASYNC_CRYPT */

#ifdef HAVE_SNI
    if (sniHostName)
        if (wolfSSL_CTX_UseSNI(ctx, 0, sniHostName,
                    (word16) XSTRLEN(sniHostName)) != WOLFSSL_SUCCESS) {
            wolfSSL_CTX_free(ctx);
            err_sys("UseSNI failed");
    }
#endif
#ifdef HAVE_MAX_FRAGMENT
    if (maxFragment)
        if (wolfSSL_CTX_UseMaxFragment(ctx, maxFragment) != WOLFSSL_SUCCESS) {
            wolfSSL_CTX_free(ctx);
            err_sys("UseMaxFragment failed");
        }
#endif
#ifdef HAVE_TRUNCATED_HMAC
    if (truncatedHMAC)
        if (wolfSSL_CTX_UseTruncatedHMAC(ctx) != WOLFSSL_SUCCESS) {
            wolfSSL_CTX_free(ctx);
            err_sys("UseTruncatedHMAC failed");
        }
#endif
#ifdef HAVE_SESSION_TICKET
    if (wolfSSL_CTX_UseSessionTicket(ctx) != WOLFSSL_SUCCESS) {
        wolfSSL_CTX_free(ctx);
        err_sys("UseSessionTicket failed");
    }
#endif
#ifdef HAVE_EXTENDED_MASTER
    if (disableExtMasterSecret)
        if (wolfSSL_CTX_DisableExtendedMasterSecret(ctx) != WOLFSSL_SUCCESS) {
            wolfSSL_CTX_free(ctx);
            err_sys("DisableExtendedMasterSecret failed");
        }
#endif
#if defined(HAVE_CURVE25519) && defined(HAVE_SUPPORTED_CURVES)
    if (useX25519) {
        if (wolfSSL_CTX_UseSupportedCurve(ctx, WOLFSSL_ECC_X25519)
                                                           != WOLFSSL_SUCCESS) {
            err_sys("unable to support X25519");
        }
        if (wolfSSL_CTX_UseSupportedCurve(ctx, WOLFSSL_ECC_SECP256R1)
                                                           != WOLFSSL_SUCCESS) {
            err_sys("unable to support secp256r1");
        }
        if (wolfSSL_CTX_UseSupportedCurve(ctx, WOLFSSL_ECC_SECP384R1)
                                                           != WOLFSSL_SUCCESS) {
            err_sys("unable to support secp384r1");
        }
    }
#endif /* HAVE_CURVE25519 && HAVE_SUPPORTED_CURVES */

#ifdef WOLFSSL_TLS13
    if (noPskDheKe)
        wolfSSL_CTX_no_dhe_psk(ctx);
#endif
#if defined(WOLFSSL_TLS13) && defined(WOLFSSL_POST_HANDSHAKE_AUTH)
    if (postHandAuth)
        wolfSSL_CTX_allow_post_handshake_auth(ctx);
#endif

    if (benchmark) {
        ((func_args*)args)->return_code =
            ClientBenchmarkConnections(ctx, host, port, dtlsUDP, dtlsSCTP,
                                       benchmark, resumeSession, useX25519,
                                       helloRetry, onlyKeyShare, version);
        wolfSSL_CTX_free(ctx);
        exit(EXIT_SUCCESS);
    }

    if(throughput) {
        ((func_args*)args)->return_code =
            ClientBenchmarkThroughput(ctx, host, port, dtlsUDP, dtlsSCTP,
                                      block, throughput, useX25519);
        wolfSSL_CTX_free(ctx);
        exit(EXIT_SUCCESS);
    }

    #if defined(WOLFSSL_MDK_ARM)
    wolfSSL_CTX_set_verify(ctx, WOLFSSL_VERIFY_NONE, 0);
    #endif

    #if defined(OPENSSL_EXTRA)
    if (wolfSSL_CTX_get_read_ahead(ctx) != 0) {
        wolfSSL_CTX_free(ctx);
        err_sys("bad read ahead default value");
    }
    if (wolfSSL_CTX_set_read_ahead(ctx, 1) != WOLFSSL_SUCCESS) {
        wolfSSL_CTX_free(ctx);
        err_sys("error setting read ahead value");
    }
    #endif

#if defined(WOLFSSL_STATIC_MEMORY) && defined(DEBUG_WOLFSSL)
        fprintf(stderr, "Before creating SSL\n");
        if (wolfSSL_CTX_is_static_memory(ctx, &mem_stats) != 1)
            err_sys("ctx not using static memory");
        if (wolfSSL_PrintStats(&mem_stats) != 1) /* function in test.h */
            err_sys("error printing out memory stats");
#endif

    if (doMcast) {
#ifdef WOLFSSL_MULTICAST
        wolfSSL_CTX_mcast_set_member_id(ctx, mcastID);
        if (wolfSSL_CTX_set_cipher_list(ctx, "WDM-NULL-SHA256")
                                                           != WOLFSSL_SUCCESS) {
            wolfSSL_CTX_free(ctx);
            err_sys("Couldn't set multicast cipher list.");
        }
#endif
    }

#ifdef HAVE_PK_CALLBACKS
    if (pkCallbacks)
        SetupPkCallbacks(ctx);
#endif

    ssl = wolfSSL_new(ctx);
    if (ssl == NULL) {
        wolfSSL_CTX_free(ctx);
        err_sys("unable to get SSL object");
    }

    #ifdef OPENSSL_EXTRA
    wolfSSL_KeepArrays(ssl);
    #endif

#if defined(WOLFSSL_STATIC_MEMORY) && defined(DEBUG_WOLFSSL)
        fprintf(stderr, "After creating SSL\n");
        if (wolfSSL_CTX_is_static_memory(ctx, &mem_stats) != 1)
            err_sys("ctx not using static memory");
        if (wolfSSL_PrintStats(&mem_stats) != 1) /* function in test.h */
            err_sys("error printing out memory stats");
#endif

    #ifdef WOLFSSL_TLS13
    if (!helloRetry) {
        if (onlyKeyShare == 0 || onlyKeyShare == 2) {
        #ifdef HAVE_CURVE25519
            if (useX25519) {
                if (wolfSSL_UseKeyShare(ssl, WOLFSSL_ECC_X25519)
                                                           != WOLFSSL_SUCCESS) {
                    err_sys("unable to use curve x25519");
                }
            }
        #endif
        #ifdef HAVE_ECC
            #if defined(HAVE_ECC256) || defined(HAVE_ALL_CURVES)
            if (wolfSSL_UseKeyShare(ssl, WOLFSSL_ECC_SECP256R1)
                                                           != WOLFSSL_SUCCESS) {
                err_sys("unable to use curve secp256r1");
            }
            #endif
            #if defined(HAVE_ECC384) || defined(HAVE_ALL_CURVES)
            if (wolfSSL_UseKeyShare(ssl, WOLFSSL_ECC_SECP384R1)
                                                           != WOLFSSL_SUCCESS) {
                err_sys("unable to use curve secp384r1");
            }
            #endif
        #endif
        }
        if (onlyKeyShare == 0 || onlyKeyShare == 1) {
        #ifdef HAVE_FFDHE_2048
            if (wolfSSL_UseKeyShare(ssl, WOLFSSL_FFDHE_2048)
                                                           != WOLFSSL_SUCCESS) {
                err_sys("unable to use DH 2048-bit parameters");
            }
        #endif
        }
    }
    else {
        wolfSSL_NoKeyShares(ssl);
    }
    #endif

    if (doMcast) {
#ifdef WOLFSSL_MULTICAST
        byte pms[512]; /* pre master secret */
        byte cr[32];   /* client random */
        byte sr[32];   /* server random */
        const byte suite[2] = {0, 0xfe};  /* WDM_WITH_NULL_SHA256 */

        XMEMSET(pms, 0x23, sizeof(pms));
        XMEMSET(cr, 0xA5, sizeof(cr));
        XMEMSET(sr, 0x5A, sizeof(sr));

        if (wolfSSL_set_secret(ssl, 1, pms, sizeof(pms), cr, sr, suite)
                                                           != WOLFSSL_SUCCESS) {
            wolfSSL_CTX_free(ctx);
            err_sys("unable to set mcast secret");
        }
#endif
    }

    #ifdef HAVE_SESSION_TICKET
    wolfSSL_set_SessionTicket_cb(ssl, sessionTicketCB, (void*)"initial session");
    #endif

#ifdef HAVE_ALPN
    if (alpnList != NULL) {
       printf("ALPN accepted protocols list : %s\n", alpnList);
       wolfSSL_UseALPN(ssl, alpnList, (word32)XSTRLEN(alpnList), alpn_opt);
    }
#endif
#ifdef HAVE_CERTIFICATE_STATUS_REQUEST
    if (statusRequest) {
        if (wolfSSL_CTX_EnableOCSPStapling(ctx) != WOLFSSL_SUCCESS)
            err_sys("can't enable OCSP Stapling Certificate Manager");

        switch (statusRequest) {
            case WOLFSSL_CSR_OCSP:
                if (wolfSSL_UseOCSPStapling(ssl, WOLFSSL_CSR_OCSP,
                               WOLFSSL_CSR_OCSP_USE_NONCE) != WOLFSSL_SUCCESS) {
                    wolfSSL_free(ssl);
                    wolfSSL_CTX_free(ctx);
                    err_sys("UseCertificateStatusRequest failed");
                }

            break;
        }

        wolfSSL_CTX_EnableOCSP(ctx, 0);
    }
#endif
#ifdef HAVE_CERTIFICATE_STATUS_REQUEST_V2
    if (statusRequest) {
        if (wolfSSL_CTX_EnableOCSPStapling(ctx) != WOLFSSL_SUCCESS)
            err_sys("can't enable OCSP Stapling Certificate Manager");

        switch (statusRequest) {
            case WOLFSSL_CSR2_OCSP:
                if (wolfSSL_UseOCSPStaplingV2(ssl,
                    WOLFSSL_CSR2_OCSP, WOLFSSL_CSR2_OCSP_USE_NONCE)
                                                           != WOLFSSL_SUCCESS) {
                    wolfSSL_free(ssl);
                    wolfSSL_CTX_free(ctx);
                    err_sys("UseCertificateStatusRequest failed");
                }
            break;
            case WOLFSSL_CSR2_OCSP_MULTI:
                if (wolfSSL_UseOCSPStaplingV2(ssl,
                    WOLFSSL_CSR2_OCSP_MULTI, 0)
                                                           != WOLFSSL_SUCCESS) {
                    wolfSSL_free(ssl);
                    wolfSSL_CTX_free(ctx);
                    err_sys("UseCertificateStatusRequest failed");
                }
            break;

        }

        wolfSSL_CTX_EnableOCSP(ctx, 0);
    }
#endif

    tcp_connect(&sockfd, host, port, dtlsUDP, dtlsSCTP, ssl);
    if (wolfSSL_set_fd(ssl, sockfd) != WOLFSSL_SUCCESS) {
        wolfSSL_free(ssl);
        wolfSSL_CTX_free(ctx);
        err_sys("error in setting fd");
    }

    /* STARTTLS */
    if (doSTARTTLS) {
        if (StartTLS_Init(&sockfd) != WOLFSSL_SUCCESS) {
            wolfSSL_free(ssl);
            wolfSSL_CTX_free(ctx);
            err_sys("error during STARTTLS protocol");
        }
    }

#ifdef HAVE_CRL
    if (disableCRL == 0 && !useVerifyCb) {
    #ifdef HAVE_IO_TIMEOUT
        wolfIO_SetTimeout(DEFAULT_TIMEOUT_SEC);
    #endif

        if (wolfSSL_EnableCRL(ssl, WOLFSSL_CRL_CHECKALL) != WOLFSSL_SUCCESS) {
            wolfSSL_free(ssl);
            wolfSSL_CTX_free(ctx);
            err_sys("can't enable crl check");
        }
        if (wolfSSL_LoadCRL(ssl, crlPemDir, WOLFSSL_FILETYPE_PEM, 0)
                                                           != WOLFSSL_SUCCESS) {
            wolfSSL_free(ssl);
            wolfSSL_CTX_free(ctx);
            err_sys("can't load crl, check crlfile and date validity");
        }
        if (wolfSSL_SetCRL_Cb(ssl, CRL_CallBack) != WOLFSSL_SUCCESS) {
            wolfSSL_free(ssl);
            wolfSSL_CTX_free(ctx);
            err_sys("can't set crl callback");
        }
    }
#endif
#ifdef HAVE_SECURE_RENEGOTIATION
    if (scr) {
        if (wolfSSL_UseSecureRenegotiation(ssl) != WOLFSSL_SUCCESS) {
            wolfSSL_free(ssl);
            wolfSSL_CTX_free(ctx);
            err_sys("can't enable secure renegotiation");
        }
    }
#endif
#ifdef ATOMIC_USER
    if (atomicUser)
        SetupAtomicUser(ctx, ssl);
#endif
#ifdef HAVE_PK_CALLBACKS
    if (pkCallbacks)
        SetupPkCallbackContexts(ssl, &pkCbInfo);
#endif
    if (matchName && doPeerCheck)
        wolfSSL_check_domain_name(ssl, domain);
#ifndef WOLFSSL_CALLBACKS
    if (nonBlocking) {
#ifdef WOLFSSL_DTLS
        if (doDTLS) {
            wolfSSL_dtls_set_using_nonblock(ssl, 1);
        }
#endif
        tcp_set_nonblocking(&sockfd);
        ret = NonBlockingSSL_Connect(ssl);
    }
    else {
        do {
            err = 0; /* reset error */
            ret = wolfSSL_connect(ssl);
            if (ret != WOLFSSL_SUCCESS) {
                err = wolfSSL_get_error(ssl, 0);
            #ifdef WOLFSSL_ASYNC_CRYPT
                if (err == WC_PENDING_E) {
                    ret = wolfSSL_AsyncPoll(ssl, WOLF_POLL_FLAG_CHECK_HW);
                    if (ret < 0) break;
                }
            #endif
            }
        } while (err == WC_PENDING_E);
    }
#else
    timeout.tv_sec  = DEFAULT_TIMEOUT_SEC;
    timeout.tv_usec = 0;
    ret = NonBlockingSSL_Connect(ssl);  /* will keep retrying on timeout */
#endif
    if (ret != WOLFSSL_SUCCESS) {
        err = wolfSSL_get_error(ssl, 0);
        printf("wolfSSL_connect error %d, %s\n", err,
            wolfSSL_ERR_error_string(err, buffer));

        /* cleanup */
        wolfSSL_free(ssl);
        wolfSSL_CTX_free(ctx);
        CloseSocket(sockfd);

        if (!exitWithRet)
            err_sys("wolfSSL_connect failed");
        /* see note at top of README */
        /* if you're getting an error here  */

        ((func_args*)args)->return_code = err;
        goto exit;
    }

    showPeer(ssl);

#ifdef OPENSSL_EXTRA
    {
        byte*  rnd;
        byte*  pt;
        size_t size;

        /* get size of buffer then print */
        size = wolfSSL_get_client_random(NULL, NULL, 0);
        if (size == 0) {
            wolfSSL_free(ssl);
            wolfSSL_CTX_free(ctx);
            err_sys("error getting client random buffer size");
        }

        rnd = (byte*)XMALLOC(size, NULL, DYNAMIC_TYPE_TMP_BUFFER);
        if (rnd == NULL) {
            wolfSSL_free(ssl);
            wolfSSL_CTX_free(ctx);
            err_sys("error creating client random buffer");
        }

        size = wolfSSL_get_client_random(ssl, rnd, size);
        if (size == 0) {
            XFREE(rnd, NULL, DYNAMIC_TYPE_TMP_BUFFER);
            wolfSSL_free(ssl);
            wolfSSL_CTX_free(ctx);
            err_sys("error getting client random buffer");
        }

        printf("Client Random : ");
        for (pt = rnd; pt < rnd + size; pt++) printf("%02X", *pt);
        printf("\n");
        XFREE(rnd, NULL, DYNAMIC_TYPE_TMP_BUFFER);
    }
#endif

    if (doSTARTTLS) {
        if (XSTRNCMP(starttlsProt, "smtp", 4) == 0) {
            if (SMTP_Shutdown(ssl, wc_shutdown) != WOLFSSL_SUCCESS) {
                wolfSSL_free(ssl);
                wolfSSL_CTX_free(ctx);
                err_sys("error closing STARTTLS connection");
            }
        }

        wolfSSL_free(ssl);
        CloseSocket(sockfd);

        wolfSSL_CTX_free(ctx);

        ((func_args*)args)->return_code = 0;
        return 0;
    }

#ifdef HAVE_ALPN
    if (alpnList != NULL) {
        char *protocol_name = NULL;
        word16 protocol_nameSz = 0;

        err = wolfSSL_ALPN_GetProtocol(ssl, &protocol_name, &protocol_nameSz);
        if (err == WOLFSSL_SUCCESS)
            printf("Received ALPN protocol : %s (%d)\n",
                   protocol_name, protocol_nameSz);
        else if (err == WOLFSSL_ALPN_NOT_FOUND)
            printf("No ALPN response received (no match with server)\n");
        else
            printf("Getting ALPN protocol name failed\n");
    }
#endif

#ifdef HAVE_SECURE_RENEGOTIATION
    if (scr && forceScr) {
        if (nonBlocking) {
            printf("not doing secure renegotiation on example with"
                   " nonblocking yet");
        } else {
            if (wolfSSL_Rehandshake(ssl) != WOLFSSL_SUCCESS) {
                err = wolfSSL_get_error(ssl, 0);
                printf("err = %d, %s\n", err,
                                wolfSSL_ERR_error_string(err, buffer));
                wolfSSL_free(ssl);
                wolfSSL_CTX_free(ctx);
                err_sys("wolfSSL_Rehandshake failed");
            }
        }
    }
#endif /* HAVE_SECURE_RENEGOTIATION */

    if (sendGET) {
        printf("SSL connect ok, sending GET...\n");
        msgSz = 28;
        strncpy(msg, "GET /index.html HTTP/1.0\r\n\r\n", msgSz);
        msg[msgSz] = '\0';

        resumeSz = msgSz;
        strncpy(resumeMsg, "GET /index.html HTTP/1.0\r\n\r\n", resumeSz);
        resumeMsg[resumeSz] = '\0';
    }

/* allow some time for exporting the session */
#ifdef WOLFSSL_SESSION_EXPORT_DEBUG
#ifdef USE_WINDOWS_API
    Sleep(500);
#elif defined(WOLFSSL_TIRTOS)
    Task_sleep(1);
#else
    sleep(1);
#endif
#endif /* WOLFSSL_SESSION_EXPORT_DEBUG */

#ifdef WOLFSSL_TLS13
    if (updateKeysIVs)
        wolfSSL_update_keys(ssl);
#endif

    ClientWrite(ssl, msg, msgSz);

    ClientRead(ssl, reply, sizeof(reply)-1, 1);

#if defined(WOLFSSL_TLS13)
    if (updateKeysIVs || postHandAuth)
        ClientWrite(ssl, msg, msgSz);
#endif
    if (sendGET) {  /* get html */
        ClientRead(ssl, reply, sizeof(reply)-1, 0);
    }

#ifndef NO_SESSION_CACHE
    if (resumeSession) {
        session   = wolfSSL_get_session(ssl);
    }
#endif

    if (dtlsUDP == 0) {           /* don't send alert after "break" command */
        ret = wolfSSL_shutdown(ssl);
        if (wc_shutdown && ret == WOLFSSL_SHUTDOWN_NOT_DONE)
            wolfSSL_shutdown(ssl);    /* bidirectional shutdown */
    }
#ifdef ATOMIC_USER
    if (atomicUser)
        FreeAtomicUser(ssl);
#endif

    /* display collected statistics */
#ifdef WOLFSSL_STATIC_MEMORY
    if (wolfSSL_is_static_memory(ssl, &ssl_stats) != 1)
        err_sys("static memory was not used with ssl");

    fprintf(stderr, "\nprint off SSL memory stats\n");
    fprintf(stderr, "*** This is memory state before wolfSSL_free is called\n");
    fprintf(stderr, "peak connection memory = %d\n", ssl_stats.peakMem);
    fprintf(stderr, "current memory in use  = %d\n", ssl_stats.curMem);
    fprintf(stderr, "peak connection allocs = %d\n", ssl_stats.peakAlloc);
    fprintf(stderr, "current connection allocs = %d\n", ssl_stats.curAlloc);
    fprintf(stderr, "total connection allocs   = %d\n", ssl_stats.totalAlloc);
    fprintf(stderr, "total connection frees    = %d\n\n", ssl_stats.totalFr);
#endif

    wolfSSL_free(ssl);
    CloseSocket(sockfd);

#ifndef NO_SESSION_CACHE
    if (resumeSession) {
        sslResume = wolfSSL_new(ctx);
        if (sslResume == NULL) {
            wolfSSL_CTX_free(ctx);
            err_sys("unable to get SSL object");
        }

        if (dtlsUDP) {
#ifdef USE_WINDOWS_API
            Sleep(500);
#elif defined(WOLFSSL_TIRTOS)
            Task_sleep(1);
#else
            sleep(1);
#endif
        }
        tcp_connect(&sockfd, host, port, dtlsUDP, dtlsSCTP, sslResume);
        if (wolfSSL_set_fd(sslResume, sockfd) != WOLFSSL_SUCCESS) {
            wolfSSL_free(sslResume);
            wolfSSL_CTX_free(ctx);
            err_sys("error in setting fd");
        }
#ifdef HAVE_ALPN
        if (alpnList != NULL) {
            printf("ALPN accepted protocols list : %s\n", alpnList);
            wolfSSL_UseALPN(sslResume, alpnList, (word32)XSTRLEN(alpnList),
                            alpn_opt);
        }
#endif
#ifdef HAVE_SECURE_RENEGOTIATION
        if (scr) {
            if (wolfSSL_UseSecureRenegotiation(sslResume) != WOLFSSL_SUCCESS) {
                wolfSSL_free(sslResume);
                wolfSSL_CTX_free(ctx);
                err_sys("can't enable secure renegotiation");
            }
        }
#endif
        wolfSSL_set_session(sslResume, session);
#ifdef HAVE_SESSION_TICKET
        wolfSSL_set_SessionTicket_cb(sslResume, sessionTicketCB,
                                    (void*)"resumed session");
#endif

#ifndef WOLFSSL_CALLBACKS
        if (nonBlocking) {
#ifdef WOLFSSL_DTLS
            if (doDTLS) {
                wolfSSL_dtls_set_using_nonblock(ssl, 1);
            }
#endif
            tcp_set_nonblocking(&sockfd);
            ret = NonBlockingSSL_Connect(sslResume);
        }
        else {
    #ifdef WOLFSSL_EARLY_DATA
        #ifndef HAVE_SESSION_TICKET
            if (!usePsk) {
            }
            else
        #endif
            if (earlyData) {
                do {
                    err = 0; /* reset error */
                    ret = wolfSSL_write_early_data(sslResume, msg, msgSz,
                                                                        &msgSz);
                    if (ret <= 0) {
                        err = wolfSSL_get_error(sslResume, 0);
                    #ifdef WOLFSSL_ASYNC_CRYPT
                        if (err == WC_PENDING_E) {
                            ret = wolfSSL_AsyncPoll(sslResume,
                                                       WOLF_POLL_FLAG_CHECK_HW);
                            if (ret < 0) break;
                        }
                    #endif
                    }
                } while (err == WC_PENDING_E);
                if (ret != msgSz) {
                    printf("SSL_write_early_data msg error %d, %s\n", err,
                                         wolfSSL_ERR_error_string(err, buffer));
                    wolfSSL_free(sslResume);
                    wolfSSL_CTX_free(ctx);
                    err_sys("SSL_write_early_data failed");
                }
                do {
                    err = 0; /* reset error */
                    ret = wolfSSL_write_early_data(sslResume, msg, msgSz,
                                                                        &msgSz);
                    if (ret <= 0) {
                        err = wolfSSL_get_error(sslResume, 0);
                    #ifdef WOLFSSL_ASYNC_CRYPT
                        if (err == WC_PENDING_E) {
                            ret = wolfSSL_AsyncPoll(sslResume,
                                                       WOLF_POLL_FLAG_CHECK_HW);
                            if (ret < 0) break;
                        }
                    #endif
                    }
                } while (err == WC_PENDING_E);
                if (ret != msgSz) {
                    printf("SSL_write_early_data msg error %d, %s\n", err,
                                         wolfSSL_ERR_error_string(err, buffer));
                    wolfSSL_free(sslResume);
                    wolfSSL_CTX_free(ctx);
                    err_sys("SSL_write_early_data failed");
                }
            }
    #endif
            do {
                err = 0; /* reset error */
                ret = wolfSSL_connect(sslResume);
                if (ret != WOLFSSL_SUCCESS) {
                    err = wolfSSL_get_error(sslResume, 0);
                #ifdef WOLFSSL_ASYNC_CRYPT
                    if (err == WC_PENDING_E) {
                        ret = wolfSSL_AsyncPoll(sslResume,
                                                    WOLF_POLL_FLAG_CHECK_HW);
                        if (ret < 0) break;
                    }
                #endif
                }
            } while (err == WC_PENDING_E);
        }
#else
        timeout.tv_sec  = DEFAULT_TIMEOUT_SEC;
        timeout.tv_usec = 0;
        ret = NonBlockingSSL_Connect(sslResume);  /* will keep retrying on timeout */
#endif
        if (ret != WOLFSSL_SUCCESS) {
            printf("wolfSSL_connect resume error %d, %s\n", err,
                wolfSSL_ERR_error_string(err, buffer));
            wolfSSL_free(sslResume);
            wolfSSL_CTX_free(ctx);
            err_sys("wolfSSL_connect resume failed");
        }

        showPeer(sslResume);

        if (wolfSSL_session_reused(sslResume))
            printf("reused session id\n");
        else
            printf("didn't reuse session id!!!\n");

#ifdef HAVE_ALPN
        if (alpnList != NULL) {
            char *protocol_name = NULL;
            word16 protocol_nameSz = 0;

            printf("Sending ALPN accepted list : %s\n", alpnList);
            err = wolfSSL_ALPN_GetProtocol(sslResume, &protocol_name,
                                           &protocol_nameSz);
            if (err == WOLFSSL_SUCCESS)
                printf("Received ALPN protocol : %s (%d)\n",
                       protocol_name, protocol_nameSz);
            else if (err == WOLFSSL_ALPN_NOT_FOUND)
                printf("Not received ALPN response (no match with server)\n");
            else
                printf("Getting ALPN protocol name failed\n");
        }
#endif

    /* allow some time for exporting the session */
    #ifdef WOLFSSL_SESSION_EXPORT_DEBUG
        #ifdef USE_WINDOWS_API
            Sleep(500);
        #elif defined(WOLFSSL_TIRTOS)
            Task_sleep(1);
        #else
            sleep(1);
        #endif
    #endif /* WOLFSSL_SESSION_EXPORT_DEBUG */

        do {
            err = 0; /* reset error */
            ret = wolfSSL_write(sslResume, resumeMsg, resumeSz);
            if (ret <= 0) {
                err = wolfSSL_get_error(sslResume, 0);
            #ifdef WOLFSSL_ASYNC_CRYPT
                if (err == WC_PENDING_E) {
                    ret = wolfSSL_AsyncPoll(sslResume, WOLF_POLL_FLAG_CHECK_HW);
                    if (ret < 0) break;
                }
            #endif
            }
        } while (err == WC_PENDING_E);
        if (ret != resumeSz) {
            printf("SSL_write resume error %d, %s\n", err,
                    wolfSSL_ERR_error_string(err, buffer));
            wolfSSL_free(sslResume);
            wolfSSL_CTX_free(ctx);
            err_sys("SSL_write failed");
        }

        if (nonBlocking) {
            /* give server a chance to bounce a message back to client */
        #ifdef USE_WINDOWS_API
            Sleep(500);
        #elif defined(WOLFSSL_TIRTOS)
            Task_sleep(1);
        #else
            sleep(1);
        #endif
        }

        do {
            err = 0; /* reset error */
            ret = wolfSSL_read(sslResume, reply, sizeof(reply)-1);
            if (ret <= 0) {
                err = wolfSSL_get_error(sslResume, 0);
            #ifdef WOLFSSL_ASYNC_CRYPT
                if (err == WC_PENDING_E) {
                    ret = wolfSSL_AsyncPoll(sslResume, WOLF_POLL_FLAG_CHECK_HW);
                    if (ret < 0) break;
                }
            #endif
            }
        } while (err == WC_PENDING_E);
        if (ret > 0) {
            reply[ret] = 0;
            printf("Server resume response: %s\n", reply);

            if (sendGET) {  /* get html */
                while (1) {
                    do {
                        err = 0; /* reset error */
                        ret = wolfSSL_read(sslResume, reply, sizeof(reply)-1);
                        if (ret <= 0) {
                            err = wolfSSL_get_error(sslResume, 0);
                        #ifdef WOLFSSL_ASYNC_CRYPT
                            if (err == WC_PENDING_E) {
                                ret = wolfSSL_AsyncPoll(sslResume,
                                                    WOLF_POLL_FLAG_CHECK_HW);
                                if (ret < 0) break;
                            }
                        #endif
                        }
                    } while (err == WC_PENDING_E);
                    if (ret > 0) {
                        reply[ret] = 0;
                        printf("%s\n", reply);
                    }
                    else
                        break;
                }
            }
        }
        if (ret < 0) {
            if (err != WOLFSSL_ERROR_WANT_READ) {
                printf("SSL_read resume error %d, %s\n", err,
                    wolfSSL_ERR_error_string(err, buffer));
                wolfSSL_free(sslResume);
                wolfSSL_CTX_free(ctx);
                err_sys("SSL_read failed");
            }
        }

        /* try to send session break */
        do {
            err = 0; /* reset error */
            ret = wolfSSL_write(sslResume, msg, msgSz);
            if (ret <= 0) {
                err = wolfSSL_get_error(sslResume, 0);
            #ifdef WOLFSSL_ASYNC_CRYPT
                if (err == WC_PENDING_E) {
                    ret = wolfSSL_AsyncPoll(sslResume, WOLF_POLL_FLAG_CHECK_HW);
                    if (ret < 0) break;
                }
            #endif
            }
        } while (err == WC_PENDING_E);

        ret = wolfSSL_shutdown(sslResume);
        if (wc_shutdown && ret == WOLFSSL_SHUTDOWN_NOT_DONE)
            wolfSSL_shutdown(sslResume);    /* bidirectional shutdown */

        /* display collected statistics */
    #ifdef WOLFSSL_STATIC_MEMORY
        if (wolfSSL_is_static_memory(sslResume, &ssl_stats) != 1)
            err_sys("static memory was not used with ssl");

        fprintf(stderr, "\nprint off SSLresume memory stats\n");
        fprintf(stderr, "*** This is memory state before wolfSSL_free is called\n");
        fprintf(stderr, "peak connection memory = %d\n", ssl_stats.peakMem);
        fprintf(stderr, "current memory in use  = %d\n", ssl_stats.curMem);
        fprintf(stderr, "peak connection allocs = %d\n", ssl_stats.peakAlloc);
        fprintf(stderr, "current connection allocs = %d\n", ssl_stats.curAlloc);
        fprintf(stderr, "total connection allocs   = %d\n", ssl_stats.totalAlloc);
        fprintf(stderr, "total connection frees    = %d\n\n", ssl_stats.totalFr);
    #endif

        wolfSSL_free(sslResume);
        CloseSocket(sockfd);
    }
#endif /* NO_SESSION_CACHE */

    wolfSSL_CTX_free(ctx);

    ((func_args*)args)->return_code = 0;

exit:

#ifdef WOLFSSL_ASYNC_CRYPT
    wolfAsync_DevClose(&devId);
#endif

    /* There are use cases  when these assignments are not read. To avoid
     * potential confusion those warnings have been handled here.
     */
    (void) overrideDateErrors;
    (void) useClientCert;
    (void) verifyCert;
    (void) ourCert;
    (void) ourKey;
    (void) useVerifyCb;

#if !defined(WOLFSSL_TIRTOS)
    return 0;
#endif
}

#endif /* !NO_WOLFSSL_CLIENT */


/* so overall tests can pull in test function */
#ifndef NO_MAIN_DRIVER

    int main(int argc, char** argv)
    {
        func_args args;


        StartTCP();

        args.argc = argc;
        args.argv = argv;
        args.return_code = 0;

#if defined(DEBUG_WOLFSSL) && !defined(WOLFSSL_MDK_SHELL) && !defined(STACK_TRAP)
        wolfSSL_Debugging_ON();
#endif
        wolfSSL_Init();
        ChangeToWolfRoot();

#ifndef NO_WOLFSSL_CLIENT
#ifdef HAVE_STACK_SIZE
        StackSizeCheck(&args, client_test);
#else
        client_test(&args);
#endif
#else
        printf("Client not compiled in!\n");
#endif
        wolfSSL_Cleanup();

#ifdef HAVE_WNR
    if (wc_FreeNetRandom() < 0)
        err_sys("Failed to free netRandom context");
#endif /* HAVE_WNR */

        return args.return_code;
    }

    int myoptind = 0;
    char* myoptarg = NULL;

#endif /* NO_MAIN_DRIVER */
