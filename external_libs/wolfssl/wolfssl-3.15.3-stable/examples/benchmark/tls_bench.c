/* tls_bench.c
 *
 * Copyright (C) 2006-2017 wolfSSL Inc.
 *
 * This file is part of wolfSSL. (formerly known as CyaSSL)
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */


/*
Example gcc build statement
gcc -lwolfssl -lpthread -o tls_bench tls_bench.c
./tls_bench

Or

#include <examples/benchmark/tls_bench.h>
bench_tls(args);
*/


#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif
#ifndef WOLFSSL_USER_SETTINGS
    #include <wolfssl/options.h>
#endif
#include <wolfssl/wolfcrypt/settings.h>
#include <wolfssl/ssl.h>

#include <wolfssl/test.h>

#include <examples/benchmark/tls_bench.h>

/* force certificate test buffers to be included via headers */
#undef  USE_CERT_BUFFERS_2048
#define USE_CERT_BUFFERS_2048
#undef  USE_CERT_BUFFERS_256
#define USE_CERT_BUFFERS_256
#include <wolfssl/certs_test.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>

/* Defaults for configuration parameters */
#define THREAD_PAIRS    1 /* Thread pairs of server/client */
#define MEM_BUFFER_SZ   (1024*16) /* Must be large enough to handle max packet size */
#define MIN_DHKEY_BITS  1024
#define RUNTIME_SEC     1
#define TEST_SIZE_BYTES (1024 * 1024)
#define TEST_PACKET_SIZE 1024
#define SHOW_VERBOSE 0 /* Default output is tab delimited format */

#if !defined(NO_WOLFSSL_CLIENT) && !defined(NO_WOLFSSL_SERVER)
static int argShowPeerInfo = 0; /* Show more info about wolfSSL configuration */

static const char* kTestStr =
"Biodiesel cupidatat marfa, cliche aute put a bird on it incididunt elit\n"
"polaroid. Sunt tattooed bespoke reprehenderit. Sint twee organic id\n"
"marfa. Commodo veniam ad esse gastropub. 3 wolf moon sartorial vero,\n"
"plaid delectus biodiesel squid +1 vice. Post-ironic keffiyeh leggings\n"
"selfies cray fap hoodie, forage anim. Carles cupidatat shoreditch, VHS\n"
"small batch meggings kogi dolore food truck bespoke gastropub.\n"
"\n"
"Terry richardson adipisicing actually typewriter tumblr, twee whatever\n"
"four loko you probably haven't heard of them high life. Messenger bag\n"
"whatever tattooed deep v mlkshk. Brooklyn pinterest assumenda chillwave\n"
"et, banksy ullamco messenger bag umami pariatur direct trade forage.\n"
"Typewriter culpa try-hard, pariatur sint brooklyn meggings. Gentrify\n"
"food truck next level, tousled irony non semiotics PBR ethical anim cred\n"
"readymade. Mumblecore brunch lomo odd future, portland organic terry\n"
"richardson elit leggings adipisicing ennui raw denim banjo hella. Godard\n"
"mixtape polaroid, pork belly readymade organic cray typewriter helvetica\n"
"four loko whatever street art yr farm-to-table.\n"
"\n"
"Vinyl keytar vice tofu. Locavore you probably haven't heard of them pug\n"
"pickled, hella tonx labore truffaut DIY mlkshk elit cosby sweater sint\n"
"et mumblecore. Elit swag semiotics, reprehenderit DIY sartorial nisi ugh\n"
"nesciunt pug pork belly wayfarers selfies delectus. Ethical hoodie\n"
"seitan fingerstache kale chips. Terry richardson artisan williamsburg,\n"
"eiusmod fanny pack irony tonx ennui lo-fi incididunt tofu YOLO\n"
"readymade. 8-bit sed ethnic beard officia. Pour-over iphone DIY butcher,\n"
"ethnic art party qui letterpress nisi proident jean shorts mlkshk\n"
"locavore.\n"
"\n"
"Narwhal flexitarian letterpress, do gluten-free voluptate next level\n"
"banh mi tonx incididunt carles DIY. Odd future nulla 8-bit beard ut\n"
"cillum pickled velit, YOLO officia you probably haven't heard of them\n"
"trust fund gastropub. Nisi adipisicing tattooed, Austin mlkshk 90's\n"
"small batch american apparel. Put a bird on it cosby sweater before they\n"
"sold out pork belly kogi hella. Street art mollit sustainable polaroid,\n"
"DIY ethnic ea pug beard dreamcatcher cosby sweater magna scenester nisi.\n"
"Sed pork belly skateboard mollit, labore proident eiusmod. Sriracha\n"
"excepteur cosby sweater, anim deserunt laborum eu aliquip ethical et\n"
"neutra PBR selvage.\n"
"\n"
"Raw denim pork belly truffaut, irony plaid sustainable put a bird on it\n"
"next level jean shorts exercitation. Hashtag keytar whatever, nihil\n"
"authentic aliquip disrupt laborum. Tattooed selfies deserunt trust fund\n"
"wayfarers. 3 wolf moon synth church-key sartorial, gastropub leggings\n"
"tattooed. Labore high life commodo, meggings raw denim fingerstache pug\n"
"trust fund leggings seitan forage. Nostrud ullamco duis, reprehenderit\n"
"incididunt flannel sustainable helvetica pork belly pug banksy you\n"
"probably haven't heard of them nesciunt farm-to-table. Disrupt nostrud\n"
"mollit magna, sriracha sartorial helvetica.\n"
"\n"
"Nulla kogi reprehenderit, skateboard sustainable duis adipisicing viral\n"
"ad fanny pack salvia. Fanny pack trust fund you probably haven't heard\n"
"of them YOLO vice nihil. Keffiyeh cray lo-fi pinterest cardigan aliqua,\n"
"reprehenderit aute. Culpa tousled williamsburg, marfa lomo actually anim\n"
"skateboard. Iphone aliqua ugh, semiotics pariatur vero readymade\n"
"organic. Marfa squid nulla, in laborum disrupt laboris irure gastropub.\n"
"Veniam sunt food truck leggings, sint vinyl fap.\n"
"\n"
"Hella dolore pork belly, truffaut carles you probably haven't heard of\n"
"them PBR helvetica in sapiente. Fashion axe ugh bushwick american\n"
"apparel. Fingerstache sed iphone, jean shorts blue bottle nisi bushwick\n"
"flexitarian officia veniam plaid bespoke fap YOLO lo-fi. Blog\n"
"letterpress mumblecore, food truck id cray brooklyn cillum ad sed.\n"
"Assumenda chambray wayfarers vinyl mixtape sustainable. VHS vinyl\n"
"delectus, culpa williamsburg polaroid cliche swag church-key synth kogi\n"
"magna pop-up literally. Swag thundercats ennui shoreditch vegan\n"
"pitchfork neutra truffaut etsy, sed single-origin coffee craft beer.\n"
"\n"
"Odio letterpress brooklyn elit. Nulla single-origin coffee in occaecat\n"
"meggings. Irony meggings 8-bit, chillwave lo-fi adipisicing cred\n"
"dreamcatcher veniam. Put a bird on it irony umami, trust fund bushwick\n"
"locavore kale chips. Sriracha swag thundercats, chillwave disrupt\n"
"tousled beard mollit mustache leggings portland next level. Nihil esse\n"
"est, skateboard art party etsy thundercats sed dreamcatcher ut iphone\n"
"swag consectetur et. Irure skateboard banjo, nulla deserunt messenger\n"
"bag dolor terry richardson sapiente.\n";


#ifndef NO_DH
/* dh1024 p */
static const unsigned char p[] =
{
    0xE6, 0x96, 0x9D, 0x3D, 0x49, 0x5B, 0xE3, 0x2C, 0x7C, 0xF1, 0x80, 0xC3,
    0xBD, 0xD4, 0x79, 0x8E, 0x91, 0xB7, 0x81, 0x82, 0x51, 0xBB, 0x05, 0x5E,
    0x2A, 0x20, 0x64, 0x90, 0x4A, 0x79, 0xA7, 0x70, 0xFA, 0x15, 0xA2, 0x59,
    0xCB, 0xD5, 0x23, 0xA6, 0xA6, 0xEF, 0x09, 0xC4, 0x30, 0x48, 0xD5, 0xA2,
    0x2F, 0x97, 0x1F, 0x3C, 0x20, 0x12, 0x9B, 0x48, 0x00, 0x0E, 0x6E, 0xDD,
    0x06, 0x1C, 0xBC, 0x05, 0x3E, 0x37, 0x1D, 0x79, 0x4E, 0x53, 0x27, 0xDF,
    0x61, 0x1E, 0xBB, 0xBE, 0x1B, 0xAC, 0x9B, 0x5C, 0x60, 0x44, 0xCF, 0x02,
    0x3D, 0x76, 0xE0, 0x5E, 0xEA, 0x9B, 0xAD, 0x99, 0x1B, 0x13, 0xA6, 0x3C,
    0x97, 0x4E, 0x9E, 0xF1, 0x83, 0x9E, 0xB5, 0xDB, 0x12, 0x51, 0x36, 0xF7,
    0x26, 0x2E, 0x56, 0xA8, 0x87, 0x15, 0x38, 0xDF, 0xD8, 0x23, 0xC6, 0x50,
    0x50, 0x85, 0xE2, 0x1F, 0x0D, 0xD5, 0xC8, 0x6B,
};

/* dh1024 g */
static const unsigned char g[] =
{
    0x02,
};
#endif

typedef struct {
    unsigned char buf[MEM_BUFFER_SZ];
    int write_bytes;
    int write_idx;
    int read_bytes;
    int read_idx;

    pthread_t tid;
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    int done;
} memBuf_t;

typedef struct {
    double connTime;
    double rxTime;
    double txTime;
    int connCount;
    int rxTotal;
    int txTotal;
} stats_t;

typedef struct {
    const char* cipher;

    /* The total number of bytes to transfer per connection */
    int numBytes;

    /* The data payload size in the packet. Will be padded if packet size > buffer size. */
    int packetSize;

    /* client messages to server in memory */
    memBuf_t to_server;

    /* server messages to client in memory */
    memBuf_t to_client;

    /* server */
    stats_t server_stats;

    /* client */
    stats_t client_stats;

    int shutdown;
} info_t;

/* Global vars for argument parsing */
int myoptind = 0;
char* myoptarg = NULL;

/* server send callback */
static int ServerSend(WOLFSSL* ssl, char* buf, int sz, void* ctx)
{
    info_t* info = (info_t*)ctx;

    pthread_mutex_lock(&info->to_client.mutex);

    /* check for overflow */
    if (info->to_client.write_idx + sz > MEM_BUFFER_SZ) {
        pthread_mutex_unlock(&info->to_client.mutex);
        return -1;
    }

    memcpy(&info->to_client.buf[info->to_client.write_idx], buf, sz);
    info->to_client.write_idx += sz;
    info->to_client.write_bytes += sz;

    pthread_cond_signal(&info->to_client.cond);
    pthread_mutex_unlock(&info->to_client.mutex);

    (void)ssl;

    return sz;
}


/* server recv callback */
static int ServerRecv(WOLFSSL* ssl, char* buf, int sz, void* ctx)
{
    info_t* info = (info_t*)ctx;

    pthread_mutex_lock(&info->to_server.mutex);

    while (info->to_server.write_idx - info->to_server.read_idx < sz && !info->to_client.done)
        pthread_cond_wait(&info->to_server.cond, &info->to_server.mutex);

    memcpy(buf, &info->to_server.buf[info->to_server.read_idx], sz);
    info->to_server.read_idx += sz;
    info->to_server.read_bytes += sz;

    /* if the rx has caught up with pending then reset buffer positions */
    if (info->to_server.read_bytes == info->to_server.write_bytes) {
        info->to_server.read_bytes = info->to_server.read_idx = 0;
        info->to_server.write_bytes = info->to_server.write_idx = 0;
    }

    pthread_mutex_unlock(&info->to_server.mutex);

    if (info->to_client.done != 0)
        return -1;

    (void)ssl;

    return sz;
}


/* client send callback */
static int ClientSend(WOLFSSL* ssl, char* buf, int sz, void* ctx)
{
    info_t* info = (info_t*)ctx;

    pthread_mutex_lock(&info->to_server.mutex);

    /* check for overflow */
    if (info->to_client.write_idx + sz > MEM_BUFFER_SZ) {
        pthread_mutex_unlock(&info->to_server.mutex);
        return -1;
    }

    memcpy(&info->to_server.buf[info->to_server.write_idx], buf, sz);
    info->to_server.write_idx += sz;
    info->to_server.write_bytes += sz;

    pthread_cond_signal(&info->to_server.cond);
    pthread_mutex_unlock(&info->to_server.mutex);

    (void)ssl;

    return sz;
}


/* client recv callback */
static int ClientRecv(WOLFSSL* ssl, char* buf, int sz, void* ctx)
{
    info_t* info = (info_t*)ctx;

    pthread_mutex_lock(&info->to_client.mutex);

    while (info->to_client.write_idx - info->to_client.read_idx < sz)
        pthread_cond_wait(&info->to_client.cond, &info->to_client.mutex);

    memcpy(buf, &info->to_client.buf[info->to_client.read_idx], sz);
    info->to_client.read_idx += sz;
    info->to_client.read_bytes += sz;

    /* if the rx has caught up with pending then reset buffer positions */
    if (info->to_client.read_bytes == info->to_client.write_bytes) {
        info->to_client.read_bytes = info->to_client.read_idx = 0;
        info->to_client.write_bytes = info->to_client.write_idx = 0;
    }

    pthread_mutex_unlock(&info->to_client.mutex);

    (void)ssl;

    return sz;
}


static double gettime_secs(int reset)
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    (void)reset;

    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000;
}

static void* client_thread(void* args)
{
    info_t* info = (info_t*)args;
    unsigned char* buf;
    unsigned char *writeBuf;
    double start;
    int ret, bufSize;
    WOLFSSL_CTX* cli_ctx;
    WOLFSSL* cli_ssl;
    int haveShownPeerInfo = 0;

    /* set up client */
    cli_ctx = wolfSSL_CTX_new(wolfSSLv23_client_method());
    if (cli_ctx == NULL) err_sys("error creating ctx");

#ifndef NO_CERTS
#ifdef HAVE_ECC
    if (strstr(info->cipher, "ECDSA")) {
        ret = wolfSSL_CTX_load_verify_buffer(cli_ctx, ca_ecc_cert_der_256, sizeof_ca_ecc_cert_der_256, WOLFSSL_FILETYPE_ASN1);
    }
    else
#endif
    {
        ret = wolfSSL_CTX_load_verify_buffer(cli_ctx, ca_cert_der_2048, sizeof_ca_cert_der_2048, WOLFSSL_FILETYPE_ASN1);
    }
    if (ret != WOLFSSL_SUCCESS) err_sys("error loading CA");
#endif

    wolfSSL_CTX_SetIOSend(cli_ctx, ClientSend);
    wolfSSL_CTX_SetIORecv(cli_ctx, ClientRecv);

    /* set cipher suite */
    ret = wolfSSL_CTX_set_cipher_list(cli_ctx, info->cipher);
    if (ret != WOLFSSL_SUCCESS) err_sys("error setting cipher suite");

#ifndef NO_DH
    wolfSSL_CTX_SetMinDhKey_Sz(cli_ctx, MIN_DHKEY_BITS);
#endif

    /* Allocate and initialize a packet sized buffer */
    writeBuf = (unsigned char*)malloc(info->packetSize);
    if (writeBuf != NULL) {
        strncpy((char*)writeBuf, kTestStr, info->packetSize);
        *(writeBuf + info->packetSize) = '\0';
    }
    else {
        err_sys("failed to allocate memory");
    }

    while (!info->shutdown) {
        cli_ssl = wolfSSL_new(cli_ctx);
        if (cli_ssl == NULL) err_sys("error creating client object");

        wolfSSL_SetIOReadCtx(cli_ssl, info);
        wolfSSL_SetIOWriteCtx(cli_ssl, info);

        /* perform connect */
        start = gettime_secs(1);
        ret = wolfSSL_connect(cli_ssl);
        start = gettime_secs(0) - start;
        if (ret != WOLFSSL_SUCCESS) {
            if (info->shutdown)
                break;
            err_sys("error connecting client");
        }
        info->client_stats.connTime += start;

        if ((argShowPeerInfo) && (!haveShownPeerInfo)) {
            haveShownPeerInfo = 1;
            showPeer(cli_ssl);
        }

        /* Allocate buf after handshake is complete */
        bufSize = wolfSSL_GetMaxOutputSize(cli_ssl);
        if (bufSize > 0) {
            buf = (unsigned char*)malloc(bufSize);
        }
        else {
            buf = NULL;
        }

        if (buf != NULL) {
            /* write test message to server */
            while (info->client_stats.rxTotal < info->numBytes) {
                start = gettime_secs(1);
                ret = wolfSSL_write(cli_ssl, writeBuf, info->packetSize);
                info->client_stats.txTime += gettime_secs(0) - start;
                if (ret > 0) {
                    info->client_stats.txTotal += ret;
                }

                /* read echo of message */
                start = gettime_secs(1);
                ret = wolfSSL_read(cli_ssl, buf, bufSize-1);
                info->client_stats.rxTime += gettime_secs(0) - start;
                if (ret > 0) {
                    info->client_stats.rxTotal += ret;
                }

                /* validate echo */
                if (strncmp((char*)writeBuf, (char*)buf, info->packetSize) != 0) {
                    err_sys("echo check failed!\n");
                }
            }

            free(buf);
        }
        else {
            err_sys("failed to allocate memory");
        }


        info->client_stats.connCount++;

        wolfSSL_free(cli_ssl);
    }

    /* clean up */
    wolfSSL_CTX_free(cli_ctx);
    free(writeBuf);

    pthread_cond_signal(&info->to_server.cond);
    info->to_client.done = 1;

    return NULL;
}


static void* server_thread(void* args)
{
    info_t* info = (info_t*)args;
    unsigned char *buf;
    double start;
    int ret, len = 0, bufSize;
    WOLFSSL_CTX* srv_ctx;
    WOLFSSL* srv_ssl;

    /* set up server */
    srv_ctx = wolfSSL_CTX_new(wolfSSLv23_server_method());
    if (srv_ctx == NULL) err_sys("error creating server ctx");

#ifndef NO_CERTS
#ifdef HAVE_ECC
    if (strstr(info->cipher, "ECDSA")) {
        ret = wolfSSL_CTX_use_PrivateKey_buffer(srv_ctx, ecc_key_der_256, sizeof_ecc_key_der_256, WOLFSSL_FILETYPE_ASN1);
    }
    else
#endif
    {
        ret = wolfSSL_CTX_use_PrivateKey_buffer(srv_ctx, server_key_der_2048, sizeof_server_key_der_2048, WOLFSSL_FILETYPE_ASN1);
    }
    if (ret != WOLFSSL_SUCCESS) err_sys("error loading server key");

#ifdef HAVE_ECC
    if (strstr(info->cipher, "ECDSA")) {
        ret = wolfSSL_CTX_use_certificate_buffer(srv_ctx, serv_ecc_der_256, sizeof_serv_ecc_der_256, WOLFSSL_FILETYPE_ASN1);
    }
    else
#endif
    {
        ret = wolfSSL_CTX_use_certificate_buffer(srv_ctx, server_cert_der_2048, sizeof_server_cert_der_2048, WOLFSSL_FILETYPE_ASN1);
    }
    if (ret != WOLFSSL_SUCCESS) err_sys("error loading server cert");
#endif

    wolfSSL_CTX_SetIOSend(srv_ctx, ServerSend);
    wolfSSL_CTX_SetIORecv(srv_ctx, ServerRecv);

    /* set cipher suite */
    ret = wolfSSL_CTX_set_cipher_list(srv_ctx, info->cipher);
    if (ret != WOLFSSL_SUCCESS) err_sys("error setting cipher suite");

#ifndef NO_DH
    wolfSSL_CTX_SetMinDhKey_Sz(srv_ctx, MIN_DHKEY_BITS);
    wolfSSL_CTX_SetTmpDH(srv_ctx, p, sizeof(p), g, sizeof(g));
#endif

    while (!info->shutdown) {
        srv_ssl = wolfSSL_new(srv_ctx);
        if (srv_ssl == NULL) err_sys("error creating server object");

        wolfSSL_SetIOReadCtx(srv_ssl, info);
        wolfSSL_SetIOWriteCtx(srv_ssl, info);

        /* accept tls connection without tcp sockets */
        start = gettime_secs(1);
        ret = wolfSSL_accept(srv_ssl);
        start = gettime_secs(0) - start;
        if (ret != WOLFSSL_SUCCESS) {
            if (info->shutdown)
                break;
            err_sys("error on server accept");
        }

        info->server_stats.connTime += start;

        /* Allocate buf after handshake is complete */
        bufSize = wolfSSL_GetMaxOutputSize(srv_ssl);
        if (bufSize > 0) {
            buf = (unsigned char*)malloc(bufSize);
        }
        else {
            buf = NULL;
        }

        if (buf != NULL) {
            while (info->server_stats.txTotal < info->numBytes) {
                /* read msg post handshake from client */
                memset(buf, 0, bufSize);
                start = gettime_secs(1);
                ret = wolfSSL_read(srv_ssl, buf, bufSize-1);
                info->server_stats.rxTime += gettime_secs(0) - start;
                if (ret > 0) {
                    info->server_stats.rxTotal += ret;
                    len = ret;
                }

                /* write message back to client */
                start = gettime_secs(1);
                ret = wolfSSL_write(srv_ssl, buf, len);
                info->server_stats.txTime += gettime_secs(0) - start;
                if (ret > 0) {
                    info->server_stats.txTotal += ret;
                }
            }
            free(buf);
        }
        else {
            err_sys("failed to allocate memory");
        }

        info->server_stats.connCount++;

        wolfSSL_free(srv_ssl);
    }

    /* clean up */
    wolfSSL_CTX_free(srv_ctx);

    pthread_cond_signal(&info->to_client.cond);
    info->to_server.done = 1;

    return NULL;
}

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
static void print_stats(stats_t* wcStat, const char* desc, const char* cipher, int verbose)
{
    const char* formatStr;

    if (verbose) {
        formatStr = "wolfSSL %s Benchmark on %s:\n"
               "\tTotal       : %9d bytes\n"
               "\tNum Conns   : %9d\n"
               "\tRx Total    : %9.3f ms\n"
               "\tTx Total    : %9.3f ms\n"
               "\tRx          : %9.3f MB/s\n"
               "\tTx          : %9.3f MB/s\n"
               "\tConnect     : %9.3f ms\n"
               "\tConnect Avg : %9.3f ms\n";
    }
    else {
        formatStr = "%s\t%s\t%d\t%9d\t%9.3f\t%9.3f\t%9.3f\t%9.3f\t%9.3f\t%9.3f\n";
    }

    printf(formatStr,
           desc,
           cipher,
           wcStat->txTotal + wcStat->rxTotal,
           wcStat->connCount,
           wcStat->txTime * 1000,
           wcStat->rxTime * 1000,
           wcStat->txTotal / wcStat->txTime / 1024 / 1024,
           wcStat->rxTotal / wcStat->rxTime / 1024 / 1024,
           wcStat->connTime * 1000,
           wcStat->connTime * 1000 / wcStat->connCount);
}

static void Usage(void)
{
    printf("tls_bench "    LIBWOLFSSL_VERSION_STRING
           " NOTE: All files relative to wolfSSL home dir\n");
    printf("-?          Help, print this usage\n");
    printf("-b <num>    The total <num> bytes transferred per test connection, default %d\n", TEST_SIZE_BYTES);
#ifdef DEBUG_WOLFSSL
    printf("-d          Enable debug messages\n");
#endif
    printf("-e          List Every cipher suite available\n");
    printf("-i          Show peer info\n");
    printf("-l <str>    Cipher suite list (: delimited)\n");
    printf("-t <num>    Time <num> (seconds) to run each test, default %d\n", RUNTIME_SEC);
    printf("-p <num>    The packet size <num> in bytes [1-16kB], default %d\n", TEST_PACKET_SIZE);
    printf("-v          Show verbose output\n");
    printf("-T <num>    Thread pairs of server/client, default %d\n", THREAD_PAIRS);
}

static void ShowCiphers(void)
{
    char ciphers[4096];

    int ret = wolfSSL_get_ciphers(ciphers, (int)sizeof(ciphers));

    if (ret == WOLFSSL_SUCCESS)
        printf("%s\n", ciphers);
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

int bench_tls(void* args)
{
    info_t *theadInfo, *info;
    int i, doShutdown;
    char *cipher, *next_cipher, ciphers[4096];
    int     argc = ((func_args*)args)->argc;
    char**  argv = ((func_args*)args)->argv;
    int    ch;

    /* Vars configured by command line arguments */
    int argRuntimeSec = RUNTIME_SEC;
    char *argCipherList = NULL;
    int argTestSizeBytes = TEST_SIZE_BYTES;
    int argTestPacketSize = TEST_PACKET_SIZE;
    int argThreadPairs = THREAD_PAIRS;
    int argShowVerbose = SHOW_VERBOSE;

    ((func_args*)args)->return_code = -1; /* error state */

    /* Initialize wolfSSL */
    wolfSSL_Init();

    /* Parse command line arguments */
    while ((ch = mygetopt(argc, argv, "?" "b:deil:p:t:vT:")) != -1) {
        switch (ch) {
            case '?' :
                Usage();
                exit(EXIT_SUCCESS);

            case 'b' :
                argTestSizeBytes = atoi(myoptarg);
                break;

#ifdef DEBUG_WOLFSSL
            case 'd' :
                wolfSSL_Debugging_ON();
                break;
#endif

            case 'e' :
                ShowCiphers();
                exit(EXIT_SUCCESS);

            case 'i' :
                argShowPeerInfo = 1;
                break;

            case 'l' :
                argCipherList = myoptarg;
                break;

            case 'p' :
                argTestPacketSize = atoi(myoptarg);
                break;

            case 't' :
                argRuntimeSec = atoi(myoptarg);
                break;

            case 'v' :
                argShowVerbose = 1;
                break;

            case 'T' :
                argThreadPairs = atoi(myoptarg);
                break;

            default:
                Usage();
                exit(MY_EX_USAGE);
        }
    }

    /* reset for test cases */
    myoptind = 0;

    if (argCipherList != NULL) {
        /* Use the list from CL argument */
        cipher = argCipherList;
    }
    else {
        /* Run for each cipher */
        wolfSSL_get_ciphers(ciphers, (int)sizeof(ciphers));
        cipher = ciphers;
    }

    /* Allocate test info array */
    theadInfo = (info_t*)malloc(sizeof(info_t) * argThreadPairs);
    if (theadInfo != NULL) {
        memset(theadInfo, 0, sizeof(info_t) * argThreadPairs);
    }

    /* parse by : */
    while ((cipher != NULL) && (cipher[0] != '\0') && (theadInfo != NULL)) {
        next_cipher = strchr(cipher, ':');
        if (next_cipher != NULL) {
            cipher[next_cipher - cipher] = '\0';
        }

        if (argShowVerbose) {
            printf("Cipher: %s\n", cipher);
        }

        for (i=0; i<argThreadPairs; i++) {
            info = (info_t*)memset(&theadInfo[i], 0, sizeof(info_t));

            info->cipher = cipher;
            info->numBytes = argTestSizeBytes;
            info->packetSize = argTestPacketSize;

            pthread_mutex_init(&info->to_server.mutex, NULL);
            pthread_mutex_init(&info->to_client.mutex, NULL);
            pthread_cond_init(&info->to_server.cond, NULL);
            pthread_cond_init(&info->to_client.cond, NULL);

            pthread_create(&info->to_server.tid, NULL, server_thread, info);
            pthread_create(&info->to_client.tid, NULL, client_thread, info);

            /* State that we won't be joining this thread */
            pthread_detach(info->to_server.tid);
            pthread_detach(info->to_client.tid);
        }

        /* run for x time */
        sleep(argRuntimeSec);

        /* mark threads to quit */
        for (i = 0; i < argThreadPairs; ++i) {
            info = &theadInfo[i];
            info->shutdown = 1;
        }

        /* Suspend shutdown until all threads are closed */
        do {
            doShutdown = 1;

            for (i = 0; i < argThreadPairs; ++i) {
                info = &theadInfo[i];
                if (!info->to_client.done || !info->to_server.done) {
                    doShutdown = 0;
                    sleep(1); /* Allow other threads to run */
                }

            }
        } while (!doShutdown);

        if (argShowVerbose) {
            printf("Shutdown complete\n");

            /* print results */
            for (i = 0; i < argThreadPairs; ++i) {
                info = &theadInfo[i];

                printf("\nThread %d\n", i);
                print_stats(&info->server_stats, "Server", info->cipher, 1);
                print_stats(&info->client_stats, "Server", info->cipher, 1);
            }
        }

        /* print combined results for more than one thread */
        stats_t cli_comb;
        stats_t srv_comb;
        memset(&cli_comb, 0, sizeof(cli_comb));
        memset(&srv_comb, 0, sizeof(srv_comb));

        for (i = 0; i < argThreadPairs; ++i) {
            info = &theadInfo[i];

            cli_comb.connCount += info->client_stats.connCount;
            srv_comb.connCount += info->server_stats.connCount;

            cli_comb.connTime += info->client_stats.connTime;
            srv_comb.connTime += info->server_stats.connTime;

            cli_comb.rxTotal += info->client_stats.rxTotal;
            srv_comb.rxTotal += info->server_stats.rxTotal;

            cli_comb.rxTime += info->client_stats.rxTime;
            srv_comb.rxTime += info->server_stats.rxTime;

            cli_comb.txTotal += info->client_stats.txTotal;
            srv_comb.txTotal += info->server_stats.txTotal;

            cli_comb.txTime += info->client_stats.txTime;
            srv_comb.txTime += info->server_stats.txTime;
        }

        if (argShowVerbose) {
            printf("Totals for %d Threads\n", argThreadPairs);
        }
        else {
            printf("Side\tCipher\tTotal Bytes\tNum Conns\tRx ms\tTx ms\tRx MB/s\tTx MB/s\tConnect Total ms\tConnect Avg ms\n");
            print_stats(&srv_comb, "Server", theadInfo[0].cipher, 0);
            print_stats(&cli_comb, "Client", theadInfo[0].cipher, 0);
        }

        /* target next cipher */
        cipher = (next_cipher != NULL) ? (next_cipher + 1) : NULL;
    }

    /* Cleanup the wolfSSL environment */
    wolfSSL_Cleanup();

    /* Free theadInfo array */
    free(theadInfo);

    /* Return reporting a success */
    return (((func_args*)args)->return_code = 0);
}
#endif /* !NO_WOLFSSL_CLIENT && !NO_WOLFSSL_SERVER */

#ifndef NO_MAIN_DRIVER

int main(int argc, char** argv)
{
    func_args args;

    args.argc = argc;
    args.argv = argv;
    args.return_code = 0;

#if !defined(NO_WOLFSSL_CLIENT) && !defined(NO_WOLFSSL_SERVER)
    bench_tls(&args);
#endif

    return(args.return_code);
}

#endif
