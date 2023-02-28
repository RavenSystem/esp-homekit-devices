/* tls-bench.c
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
gcc -lwolfssl -lpthread -o tls-bench tls-bench.c
./tls-bench

Or

extern int bench_tls(void);
bench_tls();
*/


#ifndef WOLFSSL_USER_SETTINGS
#include <wolfssl/options.h>
#endif
#include <wolfssl/wolfcrypt/settings.h>
#include <wolfssl/ssl.h>

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

/* configuration parameters */
#define THREAD_COUNT   2
#define RUNTIME_SEC    2
#define MEM_BUFFER_SZ  (1024*5)
#define MIN_DHKEY_BITS 1024

/* default output is tab delimited format. Uncomment these to show more */
//#define SHOW_PEER_INFO
//#define SHOW_VERBOSE_OUTPUT

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
static unsigned char p[] =
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
static unsigned char g[] =
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

#ifdef SHOW_PEER_INFO
static void showPeer(WOLFSSL* ssl)
{
    WOLFSSL_CIPHER* cipher;
#ifdef HAVE_ECC
    const char *name;
#endif
#ifndef NO_DH
    int bits;
#endif

    printf("SSL version is %s\n", wolfSSL_get_version(ssl));

    cipher = wolfSSL_get_current_cipher(ssl);
    printf("SSL cipher suite is %s\n", wolfSSL_CIPHER_get_name(cipher));

#ifdef HAVE_ECC
    if ((name = wolfSSL_get_curve_name(ssl)) != NULL)
        printf("SSL curve name is %s\n", name);
#endif
#ifndef NO_DH
    if ((bits = wolfSSL_GetDhKey_Sz(ssl)) > 0)
        printf("SSL DH size is %d bits\n", bits);
#endif
    if (wolfSSL_session_reused(ssl))
        printf("SSL reused session\n");
#ifdef WOLFSSL_ALT_CERT_CHAINS
    if (wolfSSL_is_peer_alt_cert_chain(ssl))
        printf("Alternate cert chain used\n");
#endif
}
#endif

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

    return sz;
}


static void err_sys(const char* msg)
{
    printf("wolfSSL error: %s\n", msg);
    exit(1);
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
    unsigned char buf[MEM_BUFFER_SZ];
    double start;
    int ret, len;
    WOLFSSL_CTX* cli_ctx;
    WOLFSSL* cli_ssl;
#ifdef SHOW_PEER_INFO
    int haveShownPeerInfo = 0;
#endif

    /* set up client */
    cli_ctx = wolfSSL_CTX_new(wolfTLSv1_2_client_method());
    if (cli_ctx == NULL) err_sys("error creating ctx");

    if (strstr(info->cipher, "ECDSA"))
        ret = wolfSSL_CTX_load_verify_buffer(cli_ctx, ca_ecc_cert_der_256, sizeof_ca_ecc_cert_der_256, WOLFSSL_FILETYPE_ASN1);
    else
        ret = wolfSSL_CTX_load_verify_buffer(cli_ctx, ca_cert_der_2048, sizeof_ca_cert_der_2048, WOLFSSL_FILETYPE_ASN1);
    if (ret != SSL_SUCCESS) err_sys("error loading CA");

    wolfSSL_SetIOSend(cli_ctx, ClientSend);
    wolfSSL_SetIORecv(cli_ctx, ClientRecv);

    /* set cipher suite */
    ret = wolfSSL_CTX_set_cipher_list(cli_ctx, info->cipher);
    if (ret != SSL_SUCCESS) err_sys("error setting cipher suite");

#ifndef NO_DH
    wolfSSL_CTX_SetMinDhKey_Sz(cli_ctx, MIN_DHKEY_BITS);
#endif

    while (!info->shutdown) {
        cli_ssl = wolfSSL_new(cli_ctx);
        if (cli_ctx == NULL) err_sys("error creating client object");

        wolfSSL_SetIOReadCtx(cli_ssl, info);
        wolfSSL_SetIOWriteCtx(cli_ssl, info);

        /* perform connect */
        start = gettime_secs(1);
        ret = wolfSSL_connect(cli_ssl);
        start = gettime_secs(0) - start;
        if (ret != SSL_SUCCESS) {
            if (info->shutdown)
                break;
            err_sys("error connecting client");
        }
        info->client_stats.connTime += start;

#ifdef SHOW_PEER_INFO
        if (!haveShownPeerInfo) {
            haveShownPeerInfo = 1;
            showPeer(cli_ssl);
        }
#endif

        /* write test message to server */
        len = (int)strlen(kTestStr)+1; /* include null term */
        start = gettime_secs(1);
        ret = wolfSSL_write(cli_ssl, kTestStr, len);
        info->client_stats.txTime += gettime_secs(0) - start;
        if (ret > 0) {
            info->client_stats.txTotal += ret;
        }

        /* read echo of message */
        start = gettime_secs(1);
        ret = wolfSSL_read(cli_ssl, buf, sizeof(buf)-1);
        info->client_stats.rxTime += gettime_secs(0) - start;
        if (ret > 0) {
            info->client_stats.rxTotal += ret;
        }

        /* validate echo */
        if (strncmp(kTestStr, (char*)buf, strlen(kTestStr)) != 0) {
            err_sys("echo check failed!\n");
        }

        info->client_stats.connCount++;

        wolfSSL_free(cli_ssl);
    }

    /* clean up */
    wolfSSL_CTX_free(cli_ctx);

    pthread_cond_signal(&info->to_server.cond);
    info->to_client.done = 1;

    return NULL;
}


static void* server_thread(void* args)
{
    info_t* info = (info_t*)args;
    unsigned char buf[MEM_BUFFER_SZ];
    double start;
    int ret, len = 0;
    WOLFSSL_CTX* srv_ctx;
    WOLFSSL* srv_ssl;

    /* set up server */
    srv_ctx = wolfSSL_CTX_new(wolfTLSv1_2_server_method());
    if (srv_ctx == NULL) err_sys("error creating server ctx");

    if (strstr(info->cipher, "ECDSA"))
        ret = wolfSSL_CTX_use_PrivateKey_buffer(srv_ctx, ecc_key_der_256, sizeof_ecc_key_der_256, WOLFSSL_FILETYPE_ASN1);
    else
        ret = wolfSSL_CTX_use_PrivateKey_buffer(srv_ctx, server_key_der_2048, sizeof_server_key_der_2048, WOLFSSL_FILETYPE_ASN1);
    if (ret != SSL_SUCCESS) err_sys("error loading server key");

    if (strstr(info->cipher, "ECDSA"))
        ret = wolfSSL_CTX_use_certificate_buffer(srv_ctx, serv_ecc_der_256, sizeof_serv_ecc_der_256, WOLFSSL_FILETYPE_ASN1);
    else
        ret = wolfSSL_CTX_use_certificate_buffer(srv_ctx, server_cert_der_2048, sizeof_server_cert_der_2048, WOLFSSL_FILETYPE_ASN1);
    if (ret != SSL_SUCCESS) err_sys("error loading server cert");

    wolfSSL_SetIOSend(srv_ctx, ServerSend);
    wolfSSL_SetIORecv(srv_ctx, ServerRecv);

    /* set cipher suite */
    ret = wolfSSL_CTX_set_cipher_list(srv_ctx, info->cipher);
    if (ret != SSL_SUCCESS) err_sys("error setting cipher suite");

#ifndef NO_DH
    wolfSSL_CTX_SetMinDhKey_Sz(srv_ctx, MIN_DHKEY_BITS);
    wolfSSL_CTX_SetTmpDH(srv_ctx, p, sizeof(p), g, sizeof(g));
#endif

    while (!info->shutdown) {
        srv_ssl = wolfSSL_new(srv_ctx);
        if (srv_ctx == NULL) err_sys("error creating server object");

        wolfSSL_SetIOReadCtx(srv_ssl, info);
        wolfSSL_SetIOWriteCtx(srv_ssl, info);

        /* accept tls connection without tcp sockets */
        start = gettime_secs(1);
        ret = wolfSSL_accept(srv_ssl);
        start = gettime_secs(0) - start;
        if (ret != SSL_SUCCESS) {
            if (info->shutdown)
                break;
            err_sys("error on server accept");
        }

        info->server_stats.connTime += start;

        /* read msg post handshake from client */
        memset(buf, 0, sizeof(buf));
        start = gettime_secs(1);
        ret = wolfSSL_read(srv_ssl, buf, sizeof(buf)-1);
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

        info->server_stats.connCount++;

        wolfSSL_free(srv_ssl);
    }

    /* clean up */
    wolfSSL_CTX_free(srv_ctx);

    pthread_cond_signal(&info->to_client.cond);
    info->to_server.done = 1;

    return NULL;
}

static void print_stats(stats_t* stat, const char* desc, const char* cipher, int verbose)
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
           stat->txTotal + stat->rxTotal,
           stat->connCount,
           stat->txTime * 1000,
           stat->rxTime * 1000,
           stat->txTotal / stat->txTime / 1024 / 1024,
           stat->rxTotal / stat->rxTime / 1024 / 1024,
           stat->connTime * 1000,
           stat->connTime * 1000 / stat->connCount);
}

int bench_tls(void)
{
    info_t theadInfo[THREAD_COUNT];
    info_t* info;
    int i, shutdown;
    char *cipher, *next_cipher, ciphers[4096];

#ifdef DEBUG_WOLFSSL
    wolfSSL_Debugging_ON();
#endif

    /* Initialize wolfSSL */
    wolfSSL_Init();

    /* Run for each cipher */
    wolfSSL_get_ciphers(ciphers, (int)sizeof(ciphers));

#ifndef SHOW_VERBOSE_OUTPUT
    printf("Side\tCipher\tTotal Bytes\tNum Conns\tRx ms\tTx ms\tRx MB/s\tTx MB/s\tConnect Total ms\tConnect Avg ms\n");
#endif

    /* parse by : */
    cipher = ciphers;
    while (cipher != NULL && cipher[0] != '\0') {
        next_cipher = strchr(cipher, ':');
        if (next_cipher != NULL) {
            cipher[next_cipher - cipher] = '\0';
        }

    #ifdef SHOW_VERBOSE_OUTPUT
        printf("Cipher: %s\n", cipher);
    #endif

        memset(&theadInfo, 0, sizeof(theadInfo));
        for (i=0; i<THREAD_COUNT; i++) {
            info = &theadInfo[i];

            info->cipher = cipher;

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
        sleep(RUNTIME_SEC);

        /* mark threads to quit */
        for (i = 0; i < THREAD_COUNT; ++i) {
            info = &theadInfo[i];
            info->shutdown = 1;
        }

        /* Suspend shutdown until all threads are closed */
        do {
            shutdown = 1;

            for (i = 0; i < THREAD_COUNT; ++i) {
                info = &theadInfo[i];
                if (!info->to_client.done || !info->to_server.done) {
                    shutdown = 0;
                }
            }
        } while (!shutdown);

#ifdef SHOW_VERBOSE_OUTPUT
        printf("Shutdown complete\n");

        /* print results */
        for (i = 0; i < THREAD_COUNT; ++i) {
            info = &theadInfo[i];

            printf("\nThread %d\n", i);
            print_stats(&info->server_stats, "Server", info->cipher, 1);
            print_stats(&info->client_stats, "Server", info->cipher, 1);
        }
#endif

        /* print combined results for more than one thread */
        {
            stats_t cli_comb;
            stats_t srv_comb;
            memset(&cli_comb, 0, sizeof(cli_comb));
            memset(&srv_comb, 0, sizeof(srv_comb));

            for (i = 0; i < THREAD_COUNT; ++i) {
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

        #ifdef SHOW_VERBOSE_OUTPUT
            printf("Totals for %d Threads\n", THREAD_COUNT);
        #endif
            print_stats(&srv_comb, "Server", theadInfo[0].cipher, 0);
            print_stats(&cli_comb, "Client", theadInfo[0].cipher, 0);
        }

        /* target next cipher */
        cipher = (next_cipher) ? next_cipher+1 : NULL;
    }

    /* Cleanup and return */
    wolfSSL_Cleanup();      /* Cleanup the wolfSSL environment          */

    return 0;               /* Return reporting a success               */
}

#ifndef NO_MAIN_DRIVER

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    bench_tls();

    return 0;
}

#endif
