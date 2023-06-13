/* server.c
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
#include <cyassl/ssl.h> /* name change portability layer */

#include <cyassl/ctaocrypt/settings.h>
#ifdef HAVE_ECC
    #include <cyassl/ctaocrypt/ecc.h>   /* ecc_fp_free */
#endif

#if defined(WOLFSSL_MDK_ARM) || defined(WOLFSSL_KEIL_TCP_NET)
        #include <stdio.h>
        #include <string.h>
        #include "cmsis_os.h"
        #include "rl_fs.h"
        #include "rl_net.h"
        #include "wolfssl_MDK_ARM.h"
#endif

#include <cyassl/openssl/ssl.h>
#include <cyassl/test.h>
#ifdef CYASSL_DTLS
    #include <cyassl/error-ssl.h>
#endif

#include "examples/server/server.h"

#ifndef NO_WOLFSSL_SERVER

#ifdef WOLFSSL_ASYNC_CRYPT
    static int devId = INVALID_DEVID;
#endif

/* Note on using port 0: if the server uses port 0 to bind an ephemeral port
 * number and is using the ready file for scripted testing, the code in
 * test.h will write the actual port number into the ready file for use
 * by the client. */

static const char webServerMsg[] =
    "HTTP/1.1 200 OK\n"
    "Content-Type: text/html\n"
    "Connection: close\n"
    "\n"
    "<html>\n"
    "<head>\n"
    "<title>Welcome to wolfSSL!</title>\n"
    "</head>\n"
    "<body>\n"
    "<p>wolfSSL has successfully performed handshake!</p>\n"
    "</body>\n"
    "</html>\n";

int runWithErrors = 0; /* Used with -x flag to run err_sys vs. print errors */


#ifdef CYASSL_CALLBACKS
    Timeval srvTo;
    static int srvHandShakeCB(HandShakeInfo* info)
    {
        (void)info;
        return 0;
    }

    static int srvTimeoutCB(TimeoutInfo* info)
    {
        (void)info;
        return 0;
    }

#endif

#ifndef NO_HANDSHAKE_DONE_CB
    static int myHsDoneCb(WOLFSSL* ssl, void* user_ctx)
    {
        (void)user_ctx;
        (void)ssl;

        /* printf("Notified HandShake done\n"); */

        /* return negative number to end TLS connection now */
        return 0;
    }
#endif


static void err_sys_ex(int out, const char* msg)
{
    if (out == 1) { /* if server is running w/ -x flag, print error w/o exit */
        printf("wolfSSL error: %s\n", msg);
        printf("Continuing server execution...\n\n");
    } else {
        err_sys(msg);
    }
}

static int NonBlockingSSL_Accept(SSL* ssl)
{
#ifndef CYASSL_CALLBACKS
    int ret = SSL_accept(ssl);
#else
    int ret = CyaSSL_accept_ex(ssl, srvHandShakeCB, srvTimeoutCB, srvTo);
#endif
    int error = SSL_get_error(ssl, 0);
    SOCKET_T sockfd = (SOCKET_T)CyaSSL_get_fd(ssl);
    int select_ret = 0;

    while (ret != WOLFSSL_SUCCESS &&
        (error == WOLFSSL_ERROR_WANT_READ || error == WOLFSSL_ERROR_WANT_WRITE
        #ifdef WOLFSSL_ASYNC_CRYPT
            || error == WC_PENDING_E
        #endif
    )) {
        int currTimeout = 1;

        if (error == WOLFSSL_ERROR_WANT_READ) {
            /* printf("... server would read block\n"); */
        }
        else if (error == WOLFSSL_ERROR_WANT_WRITE) {
            /* printf("... server would write block\n"); */
        }

    #ifdef WOLFSSL_ASYNC_CRYPT
        if (error == WC_PENDING_E) {
            ret = wolfSSL_AsyncPoll(ssl, WOLF_POLL_FLAG_CHECK_HW);
            if (ret < 0) break;
        }
        else
    #endif
        {
        #ifdef CYASSL_DTLS
            currTimeout = CyaSSL_dtls_get_current_timeout(ssl);
        #endif
            select_ret = tcp_select(sockfd, currTimeout);
        }

        if ((select_ret == TEST_RECV_READY) || (select_ret == TEST_ERROR_READY)
        #ifdef WOLFSSL_ASYNC_CRYPT
            || error == WC_PENDING_E
        #endif
        ) {
            #ifndef CYASSL_CALLBACKS
                ret = SSL_accept(ssl);
            #else
                ret = CyaSSL_accept_ex(ssl,
                                    srvHandShakeCB, srvTimeoutCB, srvTo);
            #endif
            error = SSL_get_error(ssl, 0);
        }
        else if (select_ret == TEST_TIMEOUT && !CyaSSL_dtls(ssl)) {
            error = WOLFSSL_ERROR_WANT_READ;
        }
    #ifdef CYASSL_DTLS
        else if (select_ret == TEST_TIMEOUT && CyaSSL_dtls(ssl) &&
                                            CyaSSL_dtls_got_timeout(ssl) >= 0) {
            error = WOLFSSL_ERROR_WANT_READ;
        }
    #endif
        else {
            error = WOLFSSL_FATAL_ERROR;
        }
    }

    return ret;
}

/* Echo number of bytes specified by -e arg */
int ServerEchoData(SSL* ssl, int clientfd, int echoData, int block,
                   int throughput)
{
    int ret = 0, err;
    double start = 0, rx_time = 0, tx_time = 0;
    int xfer_bytes = 0, select_ret, len, rx_pos;
    char* buffer;

    buffer = (char*)malloc(block);
    if (!buffer) {
        err_sys_ex(runWithErrors, "Server buffer malloc failed");
    }

    while ((echoData && throughput == 0) ||
          (!echoData && xfer_bytes < throughput))
    {
        select_ret = tcp_select(clientfd, 1); /* Timeout=1 second */
        if (select_ret == TEST_RECV_READY) {

            len = min(block, throughput - xfer_bytes);
            rx_pos = 0;

            if (throughput) {
                start = current_time(1);
            }

            /* Read data */
            while (rx_pos < len) {
                ret = SSL_read(ssl, &buffer[rx_pos], len - rx_pos);
                if (ret < 0) {
                    err = SSL_get_error(ssl, 0);
                #ifdef WOLFSSL_ASYNC_CRYPT
                    if (err == WC_PENDING_E) {
                        ret = wolfSSL_AsyncPoll(ssl, WOLF_POLL_FLAG_CHECK_HW);
                        if (ret < 0) break;
                    }
                    else
                #endif
                    if (err != WOLFSSL_ERROR_WANT_READ &&
                                             err != WOLFSSL_ERROR_ZERO_RETURN) {
                        printf("SSL_read echo error %d\n", err);
                        err_sys_ex(runWithErrors, "SSL_read failed");
                    }
                }
                else {
                    rx_pos += ret;
                }
            }
            if (throughput) {
                rx_time += current_time(0) - start;
                start = current_time(1);
            }

            /* Write data */
            do {
                err = 0; /* reset error */
                ret = SSL_write(ssl, buffer, len);
                if (ret <= 0) {
                    err = SSL_get_error(ssl, 0);
                #ifdef WOLFSSL_ASYNC_CRYPT
                    if (err == WC_PENDING_E) {
                        ret = wolfSSL_AsyncPoll(ssl, WOLF_POLL_FLAG_CHECK_HW);
                        if (ret < 0) break;
                    }
                #endif
                }
            } while (err == WC_PENDING_E);
            if (ret != len) {
                printf("SSL_write echo error %d\n", err);
                err_sys_ex(runWithErrors, "SSL_write failed");
            }

            if (throughput) {
                tx_time += current_time(0) - start;
            }

            xfer_bytes += len;
        }
    }

    free(buffer);

    if (throughput) {
        printf("wolfSSL Server Benchmark %d bytes\n"
            "\tRX      %8.3f ms (%8.3f MBps)\n"
            "\tTX      %8.3f ms (%8.3f MBps)\n",
            throughput,
            tx_time * 1000, throughput / tx_time / 1024 / 1024,
            rx_time * 1000, throughput / rx_time / 1024 / 1024
        );
    }

    return EXIT_SUCCESS;
}

static void ServerRead(WOLFSSL* ssl, char* input, int inputLen)
{
    int ret, err;
    char buffer[CYASSL_MAX_ERROR_SZ];

    /* Read data */
    do {
        err = 0; /* reset error */
        ret = SSL_read(ssl, input, inputLen);
        if (ret < 0) {
            err = SSL_get_error(ssl, 0);

        #ifdef WOLFSSL_ASYNC_CRYPT
            if (err == WC_PENDING_E) {
                ret = wolfSSL_AsyncPoll(ssl, WOLF_POLL_FLAG_CHECK_HW);
                if (ret < 0) break;
            }
            else
        #endif
        #ifdef CYASSL_DTLS
            if (wolfSSL_dtls(ssl) && err == DECRYPT_ERROR) {
                printf("Dropped client's message due to a bad MAC\n");
            }
            else
        #endif
            if (err != WOLFSSL_ERROR_WANT_READ) {
                printf("SSL_read input error %d, %s\n", err,
                                                 ERR_error_string(err, buffer));
                err_sys_ex(runWithErrors, "SSL_read failed");
            }
        }
    } while (err == WC_PENDING_E || err == WOLFSSL_ERROR_WANT_READ);
    if (ret > 0) {
        input[ret] = 0; /* null terminate message */
        printf("Client message: %s\n", input);
    }
}

static void ServerWrite(WOLFSSL* ssl, const char* output, int outputLen)
{
    int ret, err;
    char buffer[CYASSL_MAX_ERROR_SZ];

    do {
        err = 0; /* reset error */
        ret = SSL_write(ssl, output, outputLen);
        if (ret <= 0) {
            err = SSL_get_error(ssl, 0);

        #ifdef WOLFSSL_ASYNC_CRYPT
            if (err == WC_PENDING_E) {
                ret = wolfSSL_AsyncPoll(ssl, WOLF_POLL_FLAG_CHECK_HW);
                if (ret < 0) break;
            }
        #endif
        }
    } while (err == WC_PENDING_E || err == WOLFSSL_ERROR_WANT_WRITE);
    if (ret != outputLen) {
        printf("SSL_write msg error %d, %s\n", err,
                                                 ERR_error_string(err, buffer));
        err_sys_ex(runWithErrors, "SSL_write failed");
    }
}

static void Usage(void)
{
    printf("server "    LIBCYASSL_VERSION_STRING
           " NOTE: All files relative to wolfSSL home dir\n");
    printf("-?          Help, print this usage\n");
    printf("-p <num>    Port to listen on, not 0, default %d\n", yasslPort);
#ifndef WOLFSSL_TLS13
    printf("-v <num>    SSL version [0-3], SSLv3(0) - TLS1.2(3)), default %d\n",
                                 SERVER_DEFAULT_VERSION);
#else
    printf("-v <num>    SSL version [0-4], SSLv3(0) - TLS1.3(4)), default %d\n",
                                 SERVER_DEFAULT_VERSION);
#endif
    printf("-l <str>    Cipher suite list (: delimited)\n");
    printf("-c <file>   Certificate file,           default %s\n", svrCertFile);
    printf("-k <file>   Key file,                   default %s\n", svrKeyFile);
    printf("-A <file>   Certificate Authority file, default %s\n", cliCertFile);
    printf("-R <file>   Create Ready file for external monitor default none\n");
#ifndef NO_DH
    printf("-D <file>   Diffie-Hellman Params file, default %s\n", dhParamFile);
    printf("-Z <num>    Minimum DH key bits,        default %d\n",
                                 DEFAULT_MIN_DHKEY_BITS);
#endif
#ifdef HAVE_ALPN
    printf("-L <str>    Application-Layer Protocol Negotiation ({C,F}:<list>)\n");
#endif
    printf("-d          Disable client cert check\n");
    printf("-b          Bind to any interface instead of localhost only\n");
    printf("-s          Use pre Shared keys\n");
    printf("-u          Use UDP DTLS,"
           " add -v 2 for DTLSv1, -v 3 for DTLSv1.2 (default)\n");
#ifdef WOLFSSL_SCTP
    printf("-G          Use SCTP DTLS,"
           " add -v 2 for DTLSv1, -v 3 for DTLSv1.2 (default)\n");
#endif
    printf("-f          Fewer packets/group messages\n");
    printf("-r          Allow one client Resumption\n");
    printf("-N          Use Non-blocking sockets\n");
    printf("-S <str>    Use Host Name Indication\n");
    printf("-w          Wait for bidirectional shutdown\n");
#ifdef HAVE_OCSP
    printf("-o          Perform OCSP lookup on peer certificate\n");
    printf("-O <url>    Perform OCSP lookup using <url> as responder\n");
#endif
#ifdef HAVE_PK_CALLBACKS
    printf("-P          Public Key Callbacks\n");
#endif
#ifdef HAVE_ANON
    printf("-a          Anonymous server\n");
#endif
#ifndef NO_PSK
    printf("-I          Do not send PSK identity hint\n");
#endif
    printf("-x          Print server errors but do not close connection\n");
    printf("-i          Loop indefinitely (allow repeated connections)\n");
    printf("-e          Echo data mode (return raw bytes received)\n");
#ifdef HAVE_NTRU
    printf("-n          Use NTRU key (needed for NTRU suites)\n");
#endif
    printf("-B <num>    Benchmark throughput using <num> bytes and print stats\n");
#ifdef HAVE_CRL
    printf("-V          Disable CRL\n");
#endif
#ifdef WOLFSSL_TRUST_PEER_CERT
    printf("-E <file>   Path to load trusted peer cert\n");
#endif
#ifdef HAVE_WNR
    printf("-q <file>   Whitewood config file,      default %s\n", wnrConfig);
#endif
    printf("-g          Return basic HTML web page\n");
    printf("-C <num>    The number of connections to accept, default: 1\n");
    printf("-H <arg>    Internal tests [defCipherList, exitWithRet]\n");
#ifdef WOLFSSL_TLS13
    printf("-U          Update keys and IVs before sending\n");
    printf("-K          Key Exchange for PSK not using (EC)DHE\n");
#ifndef NO_DH
    printf("-y          Pre-generate Key Share using FFDHE_2048 only\n");
#endif
#ifdef HAVE_ECC
    printf("-Y          Pre-generate Key Share using P-256 only \n");
#endif
#ifdef HAVE_CURVE25519
    printf("-t          Pre-generate Key share using Curve25519 only\n");
#endif
#ifdef HAVE_SESSION_TICKET
    printf("-T          Do not generate session ticket\n");
#endif
#ifdef WOLFSSL_POST_HANDSHAKE_AUTH
    printf("-Q          Request certificate from client post-handshake\n");
#endif
#ifdef WOLFSSL_SEND_HRR_COOKIE
    printf("-J          Server sends Cookie Extension containing state\n");
#endif
#endif /* WOLFSSL_TLS13 */
#ifdef WOLFSSL_EARLY_DATA
    printf("-0          Early data read from client (0-RTT handshake)\n");
#endif
#ifdef WOLFSSL_MULTICAST
    printf("-3 <grpid>  Multicast, grpid < 256\n");
#endif
}

THREAD_RETURN CYASSL_THREAD server_test(void* args)
{
    SOCKET_T sockfd   = WOLFSSL_SOCKET_INVALID;
    SOCKET_T clientfd = WOLFSSL_SOCKET_INVALID;

    wolfSSL_method_func method = NULL;
    SSL_CTX*    ctx    = 0;
    SSL*        ssl    = 0;

#ifndef WOLFSSL_ALT_TEST_STRINGS
    const char msg[] = "I hear you fa shizzle!";
#else
    const char msg[] = "I hear you fa shizzle!\n";
#endif
    int    useWebServerMsg = 0;
    char   input[80];
    int    ch;
    int    version = SERVER_DEFAULT_VERSION;
#ifndef WOLFSSL_NO_CLIENT_AUTH
    int    doCliCertCheck = 1;
#else
    int    doCliCertCheck = 0;
#endif
#ifdef HAVE_CRL
    int    disableCRL = 0;
#endif
    int    useAnyAddr = 0;
    word16 port = wolfSSLPort;
    int    usePsk = 0;
    int    usePskPlus = 0;
    int    useAnon = 0;
    int    doDTLS = 0;
    int    dtlsUDP = 0;
    int    dtlsSCTP = 0;
    int    doMcast = 0;
    int    needDH = 0;
    int    useNtruKey   = 0;
    int    nonBlocking  = 0;
    int    fewerPackets = 0;
#ifdef HAVE_PK_CALLBACKS
    int    pkCallbacks  = 0;
    PkCbInfo pkCbInfo;
#endif
    int    wc_shutdown     = 0;
    int    resume = 0;
    int    resumeCount = 0;
    int    loops = 1;
    int    cnt = 0;
    int    echoData = 0;
    int    block = TEST_BUFFER_SIZE;
    int    throughput = 0;
    int    minDhKeyBits  = DEFAULT_MIN_DHKEY_BITS;
    short  minRsaKeyBits = DEFAULT_MIN_RSAKEY_BITS;
    short  minEccKeyBits = DEFAULT_MIN_ECCKEY_BITS;
    int    doListen = 1;
    int    crlFlags = 0;
    int    ret;
    int    err = 0;
    char*  serverReadyFile = NULL;
    char*  alpnList = NULL;
    unsigned char alpn_opt = 0;
    char*  cipherList = NULL;
    int    useDefCipherList = 0;
    const char* verifyCert = cliCertFile;
    const char* ourCert    = svrCertFile;
    const char* ourKey     = svrKeyFile;
    const char* ourDhParam = dhParamFile;
    tcp_ready*  readySignal = NULL;
    int    argc = ((func_args*)args)->argc;
    char** argv = ((func_args*)args)->argv;

#ifdef WOLFSSL_TRUST_PEER_CERT
    const char* trustCert  = NULL;
#endif

#ifndef NO_PSK
    int sendPskIdentityHint = 1;
#endif

#ifdef HAVE_SNI
    char*  sniHostName = NULL;
#endif

#ifdef HAVE_OCSP
    int    useOcsp  = 0;
    char*  ocspUrl  = NULL;
#endif

#ifdef HAVE_WNR
    const char* wnrConfigFile = wnrConfig;
#endif
    char buffer[CYASSL_MAX_ERROR_SZ];
#ifdef WOLFSSL_TLS13
    int noPskDheKe = 0;
#endif
    int updateKeysIVs = 0;
    int postHandAuth = 0;
#ifdef WOLFSSL_EARLY_DATA
    int earlyData = 0;
#endif
#ifdef WOLFSSL_SEND_HRR_COOKIE
    int hrrCookie = 0;
#endif
    byte mcastID = 0;

#ifdef WOLFSSL_STATIC_MEMORY
    #if (defined(HAVE_ECC) && !defined(ALT_ECC_SIZE)) \
        || defined(SESSION_CERTS)
        /* big enough to handle most cases including session certs */
        byte memory[204000];
    #else
        byte memory[80000];
    #endif
    byte memoryIO[34500]; /* max for IO buffer (TLS packet can be 16k) */
    WOLFSSL_MEM_CONN_STATS ssl_stats;
    #ifdef DEBUG_WOLFSSL
        WOLFSSL_MEM_STATS mem_stats;
    #endif
#endif
#ifdef WOLFSSL_TLS13
    int onlyKeyShare = 0;
    int noTicket = 0;
#endif
    int useX25519 = 0;
    int exitWithRet = 0;

    ((func_args*)args)->return_code = -1; /* error state */

#ifdef NO_RSA
    verifyCert = (char*)cliEccCertFile;
    ourCert    = (char*)eccCertFile;
    ourKey     = (char*)eccKeyFile;
#endif

    (void)needDH;
    (void)ourKey;
    (void)ourCert;
    (void)ourDhParam;
    (void)verifyCert;
    (void)useNtruKey;
    (void)doCliCertCheck;
    (void)minDhKeyBits;
    (void)minRsaKeyBits;
    (void)minEccKeyBits;
    (void)alpnList;
    (void)alpn_opt;
    (void)crlFlags;
    (void)readySignal;
    (void)updateKeysIVs;
    (void)postHandAuth;
    (void)mcastID;
    (void)useX25519;

#ifdef CYASSL_TIRTOS
    fdOpenSession(Task_self());
#endif

#ifdef WOLFSSL_VXWORKS
    useAnyAddr = 1;
#else
    /* Not Used: h, m, z, F, M, T, V, W, X */
    while ((ch = mygetopt(argc, argv, "?"
                "abc:defgijk:l:nop:q:rstuv:wxy"
                "A:B:C:D:E:GH:IJKL:NO:PQR:S:TUVYZ:"
                "03:")) != -1) {
        switch (ch) {
            case '?' :
                Usage();
                exit(EXIT_SUCCESS);

            case 'x' :
                runWithErrors = 1;
                break;

            case 'd' :
                doCliCertCheck = 0;
                break;

            case 'V' :
                #ifdef HAVE_CRL
                    disableCRL = 1;
                #endif
                break;

            case 'b' :
                useAnyAddr = 1;
                break;

            case 's' :
                usePsk = 1;
                break;

            case 'j' :
                usePskPlus = 1;
                break;

            case 'n' :
                useNtruKey = 1;
                break;

            case 'u' :
                doDTLS  = 1;
                dtlsUDP = 1;
                break;

            case 'G' :
            #ifdef WOLFSSL_SCTP
                doDTLS  = 1;
                dtlsSCTP = 1;
            #endif
                break;

            case 'f' :
                fewerPackets = 1;
                break;

            case 'R' :
                serverReadyFile = myoptarg;
                break;

            case 'r' :
                #ifndef NO_SESSION_CACHE
                    resume = 1;
                #endif
                break;

            case 'P' :
            #ifdef HAVE_PK_CALLBACKS
                pkCallbacks = 1;
            #endif
                break;

            case 'p' :
                port = (word16)atoi(myoptarg);
                break;

            case 'w' :
                wc_shutdown = 1;
                break;

            case 'v' :
                if (myoptarg[0] == 'd') {
                    version = SERVER_DOWNGRADE_VERSION;
                    break;
                }
                version = atoi(myoptarg);
                if (version < 0 || version > 4) {
                    Usage();
                    exit(MY_EX_USAGE);
                }
                break;

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

            case 'D' :
                #ifndef NO_DH
                    ourDhParam = myoptarg;
                #endif
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

            case 'N':
                nonBlocking = 1;
                break;

            case 'S' :
                #ifdef HAVE_SNI
                    sniHostName = myoptarg;
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
            case 'I':
                #ifndef NO_PSK
                    sendPskIdentityHint = 0;
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

            case 'i' :
                loops = -1;
                break;

            case 'C' :
                loops = atoi(myoptarg);
                if (loops <= 0) {
                    Usage();
                    exit(MY_EX_USAGE);
                }
                break;

            case 'e' :
                echoData = 1;
                break;

            case 'B':
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

            #ifdef WOLFSSL_TRUST_PEER_CERT
            case 'E' :
                 trustCert = myoptarg;
                break;
            #endif

            case 'q' :
                #ifdef HAVE_WNR
                    wnrConfigFile = myoptarg;
                #endif
                break;

            case 'g' :
                useWebServerMsg = 1;
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

            case 't' :
                #ifdef HAVE_CURVE25519
                    useX25519 = 1;
                    #if defined(WOLFSSL_TLS13) && defined(HAVE_ECC)
                        onlyKeyShare = 2;
                    #endif
                #endif
                break;

            case 'K' :
                #ifdef WOLFSSL_TLS13
                    noPskDheKe = 1;
                #endif
                break;

            case 'T' :
                #if defined(WOLFSSL_TLS13) && defined(HAVE_SESSION_TICKET)
                    noTicket = 1;
                #endif
                break;

            case 'U' :
                #ifdef WOLFSSL_TLS13
                    updateKeysIVs = 1;
                #endif
                break;

            case 'Q' :
            #if defined(WOLFSSL_TLS13) && defined(WOLFSSL_POST_HANDSHAKE_AUTH)
                postHandAuth = 1;
                doCliCertCheck = 0;
            #endif
                break;

            case 'J' :
            #ifdef WOLFSSL_SEND_HRR_COOKIE
                hrrCookie = 1;
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

    /* Can only use DTLS over UDP or SCTP, can't do both. */
    if (dtlsUDP && dtlsSCTP) {
        err_sys_ex(runWithErrors, "Cannot use DTLS with both UDP and SCTP.");
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
        err_sys_ex(runWithErrors, "can't load whitewood net random config file");
#endif

    switch (version) {
#ifndef NO_OLD_TLS
    #ifdef WOLFSSL_ALLOW_SSLV3
        case 0:
            method = wolfSSLv3_server_method_ex;
            break;
    #endif

    #ifndef NO_TLS
        #ifdef WOLFSSL_ALLOW_TLSV10
        case 1:
            method = wolfTLSv1_server_method_ex;
            break;
        #endif

        case 2:
            method = wolfTLSv1_1_server_method_ex;
            break;
    #endif /* !NO_TLS */
#endif /* !NO_OLD_TLS */

#ifndef NO_TLS
    #ifndef WOLFSSL_NO_TLS12
        case 3:
            method = wolfTLSv1_2_server_method_ex;
            break;
    #endif

    #ifdef WOLFSSL_TLS13
        case 4:
            method = wolfTLSv1_3_server_method_ex;
            break;
    #endif

        case SERVER_DOWNGRADE_VERSION:
            method = wolfSSLv23_server_method_ex;
            break;
#endif /* NO_TLS */

#ifdef CYASSL_DTLS
    #ifndef NO_OLD_TLS
        case -1:
            method = wolfDTLSv1_server_method_ex;
            break;
    #endif

    #ifndef WOLFSSL_NO_TLS12
        case -2:
            method = wolfDTLSv1_2_server_method_ex;
            break;
    #endif
#endif

        default:
            err_sys_ex(runWithErrors, "Bad SSL version");
    }

    if (method == NULL)
        err_sys_ex(runWithErrors, "unable to get method");

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

    if (wolfSSL_CTX_load_static_memory(&ctx, method, memory, sizeof(memory),0,1)
            != WOLFSSL_SUCCESS)
        err_sys_ex(runWithErrors, "unable to load static memory and create ctx");

    /* load in a buffer for IO */
    if (wolfSSL_CTX_load_static_memory(&ctx, NULL, memoryIO, sizeof(memoryIO),
                                 WOLFMEM_IO_POOL_FIXED | WOLFMEM_TRACK_STATS, 1)
            != WOLFSSL_SUCCESS)
        err_sys_ex(runWithErrors, "unable to load static memory and create ctx");
#else
    ctx = SSL_CTX_new(method(NULL));
#endif /* WOLFSSL_STATIC_MEMORY */
    if (ctx == NULL)
        err_sys_ex(runWithErrors, "unable to get ctx");

#if defined(HAVE_SESSION_TICKET) && defined(HAVE_CHACHA) && \
                                    defined(HAVE_POLY1305)
    if (TicketInit() != 0)
        err_sys_ex(runWithErrors, "unable to setup Session Ticket Key context");
    wolfSSL_CTX_set_TicketEncCb(ctx, myTicketEncCb);
#endif

    if (cipherList && !useDefCipherList) {
        if (SSL_CTX_set_cipher_list(ctx, cipherList) != WOLFSSL_SUCCESS)
            err_sys_ex(runWithErrors, "server can't set cipher list 1");
    }

#ifdef CYASSL_LEANPSK
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
        CyaSSL_CTX_set_group_messages(ctx);

#ifdef WOLFSSL_SCTP
    if (dtlsSCTP)
        wolfSSL_CTX_dtls_set_sctp(ctx);
#endif

#ifdef WOLFSSL_ENCRYPTED_KEYS
    SSL_CTX_set_default_passwd_cb(ctx, PasswordCallBack);
#endif

#if !defined(NO_CERTS)
    if ((!usePsk || usePskPlus) && !useAnon) {
    #if !defined(NO_FILESYSTEM)
        if (SSL_CTX_use_certificate_chain_file(ctx, ourCert)
                                         != WOLFSSL_SUCCESS)
            err_sys_ex(runWithErrors, "can't load server cert file, check file and run from"
                    " wolfSSL home dir");
    #else
        /* loads cert chain file using buffer API */
        load_buffer(ctx, ourCert, WOLFSSL_CERT_CHAIN);
    #endif
    }
#endif

#ifndef NO_DH
    if (wolfSSL_CTX_SetMinDhKey_Sz(ctx, (word16)minDhKeyBits) != WOLFSSL_SUCCESS) {
        err_sys_ex(runWithErrors, "Error setting minimum DH key size");
    }
#endif
#ifndef NO_RSA
    if (wolfSSL_CTX_SetMinRsaKey_Sz(ctx, minRsaKeyBits) != WOLFSSL_SUCCESS){
        err_sys_ex(runWithErrors, "Error setting minimum RSA key size");
    }
#endif
#ifdef HAVE_ECC
    if (wolfSSL_CTX_SetMinEccKey_Sz(ctx, minEccKeyBits) != WOLFSSL_SUCCESS){
        err_sys_ex(runWithErrors, "Error setting minimum ECC key size");
    }
#endif

#ifdef HAVE_NTRU
    if (useNtruKey) {
        if (CyaSSL_CTX_use_NTRUPrivateKey_file(ctx, ourKey)
                                != WOLFSSL_SUCCESS)
            err_sys_ex(runWithErrors, "can't load ntru key file, "
                    "Please run from wolfSSL home dir");
    }
#endif
#if !defined(NO_CERTS)
#ifdef HAVE_PK_CALLBACKS
    pkCbInfo.ourKey = ourKey;
    #ifdef TEST_PK_PRIVKEY
    if (!pkCallbacks)
    #endif
#endif
    if (!useNtruKey && (!usePsk || usePskPlus) && !useAnon) {
    #if !defined(NO_FILESYSTEM)
        if (SSL_CTX_use_PrivateKey_file(ctx, ourKey, WOLFSSL_FILETYPE_PEM)
                                         != WOLFSSL_SUCCESS)
            err_sys_ex(runWithErrors, "can't load server private key file, check file and run "
                "from wolfSSL home dir");
    #else
        /* loads private key file using buffer API */
        load_buffer(ctx, ourKey, WOLFSSL_KEY);
    #endif
    }
#endif

    if (usePsk || usePskPlus) {
#ifndef NO_PSK
        SSL_CTX_set_psk_server_callback(ctx, my_psk_server_cb);

        if (sendPskIdentityHint == 1)
            SSL_CTX_use_psk_identity_hint(ctx, "cyassl server");

        if (cipherList == NULL && !usePskPlus) {
            const char *defaultCipherList;
        #if defined(HAVE_AESGCM) && !defined(NO_DH)
            #ifdef WOLFSSL_TLS13
                defaultCipherList = "DHE-PSK-AES128-GCM-SHA256:"
                                    "TLS13-AES128-GCM-SHA256";
            #else
                defaultCipherList = "DHE-PSK-AES128-GCM-SHA256";
            #endif
                needDH = 1;
        #elif defined(HAVE_NULL_CIPHER)
                defaultCipherList = "PSK-NULL-SHA256";
        #else
                defaultCipherList = "PSK-AES128-CBC-SHA256";
        #endif
            if (SSL_CTX_set_cipher_list(ctx, defaultCipherList) != WOLFSSL_SUCCESS)
                err_sys_ex(runWithErrors, "server can't set cipher list 2");
        }
#endif
    }

    if (useAnon) {
#ifdef HAVE_ANON
        CyaSSL_CTX_allow_anon_cipher(ctx);
        if (cipherList == NULL || (cipherList && useDefCipherList)) {
            const char* defaultCipherList;
            defaultCipherList = "ADH-AES256-GCM-SHA384:"
                                "ADH-AES128-SHA";
            if (SSL_CTX_set_cipher_list(ctx, defaultCipherList)
                                                            != WOLFSSL_SUCCESS)
                err_sys_ex(runWithErrors, "server can't set cipher list 4");
        }
#endif
    }

#if !defined(NO_FILESYSTEM) && !defined(NO_CERTS)
    /* if not using PSK, verify peer with certs
       if using PSK Plus then verify peer certs except PSK suites */
    if (doCliCertCheck && (usePsk == 0 || usePskPlus) && useAnon == 0) {
        SSL_CTX_set_verify(ctx, WOLFSSL_VERIFY_PEER |
                            (usePskPlus ? WOLFSSL_VERIFY_FAIL_EXCEPT_PSK :
                                WOLFSSL_VERIFY_FAIL_IF_NO_PEER_CERT), 0);
        if (SSL_CTX_load_verify_locations(ctx, verifyCert, 0) != WOLFSSL_SUCCESS)
            err_sys_ex(runWithErrors, "can't load ca file, Please run from wolfSSL home dir");
        #ifdef WOLFSSL_TRUST_PEER_CERT
        if (trustCert) {
            if ((ret = wolfSSL_CTX_trust_peer_cert(ctx, trustCert,
                                            WOLFSSL_FILETYPE_PEM)) != WOLFSSL_SUCCESS) {
                err_sys_ex(runWithErrors, "can't load trusted peer cert file");
            }
        }
        #endif /* WOLFSSL_TRUST_PEER_CERT */
   }
#endif

#if defined(CYASSL_SNIFFER)
    /* don't use EDH, can't sniff tmp keys */
    if (cipherList == NULL) {
        if (SSL_CTX_set_cipher_list(ctx, "AES128-SHA") != WOLFSSL_SUCCESS)
            err_sys_ex(runWithErrors, "server can't set cipher list 3");
    }
#endif

#ifdef HAVE_SNI
    if (sniHostName)
        if (CyaSSL_CTX_UseSNI(ctx, CYASSL_SNI_HOST_NAME, sniHostName,
                    (word16) XSTRLEN(sniHostName)) != WOLFSSL_SUCCESS)
            err_sys_ex(runWithErrors, "UseSNI failed");
#endif

#ifdef USE_WINDOWS_API
    if (port == 0) {
        /* Generate random port for testing */
        port = GetRandomPort();
    }
#endif /* USE_WINDOWS_API */

#ifdef WOLFSSL_ASYNC_CRYPT
    ret = wolfAsync_DevOpen(&devId);
    if (ret < 0) {
        printf("Async device open failed\nRunning without async\n");
    }
    wolfSSL_CTX_UseAsync(ctx, devId);
#endif /* WOLFSSL_ASYNC_CRYPT */

#ifdef WOLFSSL_TLS13
    if (noPskDheKe)
        wolfSSL_CTX_no_dhe_psk(ctx);
    if (noTicket)
        wolfSSL_CTX_no_ticket_TLSv13(ctx);
#endif

    while (1) {
        /* allow resume option */
        if (resumeCount > 1) {
            if (dtlsUDP == 0) {
                SOCKADDR_IN_T client;
                socklen_t client_len = sizeof(client);
                clientfd = accept(sockfd, (struct sockaddr*)&client,
                                 (ACCEPT_THIRD_T)&client_len);
            }
            else {
                tcp_listen(&sockfd, &port, useAnyAddr, dtlsUDP, dtlsSCTP);
                clientfd = sockfd;
            }
            if (WOLFSSL_SOCKET_IS_INVALID(clientfd)) {
                err_sys_ex(runWithErrors, "tcp accept failed");
            }
        }
#if defined(WOLFSSL_STATIC_MEMORY) && defined(DEBUG_WOLFSSL)
        fprintf(stderr, "Before creating SSL\n");
        if (wolfSSL_CTX_is_static_memory(ctx, &mem_stats) != 1)
            err_sys_ex(runWithErrors, "ctx not using static memory");
        if (wolfSSL_PrintStats(&mem_stats) != 1) /* function in test.h */
            err_sys_ex(runWithErrors, "error printing out memory stats");
#endif

    if (doMcast) {
#ifdef WOLFSSL_MULTICAST
        wolfSSL_CTX_mcast_set_member_id(ctx, mcastID);
        if (wolfSSL_CTX_set_cipher_list(ctx, "WDM-NULL-SHA256") != WOLFSSL_SUCCESS)
            err_sys("Couldn't set multicast cipher list.");
#endif
    }

#ifdef HAVE_PK_CALLBACKS
        if (pkCallbacks)
            SetupPkCallbacks(ctx);
#endif

        ssl = SSL_new(ctx);
        if (ssl == NULL)
            err_sys_ex(runWithErrors, "unable to get SSL");
        #ifdef OPENSSL_EXTRA
        wolfSSL_KeepArrays(ssl);
        #endif

#ifdef WOLFSSL_SEND_HRR_COOKIE
        if (hrrCookie && wolfSSL_send_hrr_cookie(ssl, NULL, 0) != WOLFSSL_SUCCESS) {
            err_sys("unable to set use of cookie with HRR msg");
        }
#endif

#if defined(WOLFSSL_STATIC_MEMORY) && defined(DEBUG_WOLFSSL)
        fprintf(stderr, "After creating SSL\n");
        if (wolfSSL_CTX_is_static_memory(ctx, &mem_stats) != 1)
            err_sys_ex(runWithErrors, "ctx not using static memory");
        if (wolfSSL_PrintStats(&mem_stats) != 1) /* function in test.h */
            err_sys_ex(runWithErrors, "error printing out memory stats");
#endif

    if (doMcast) {
#ifdef WOLFSSL_MULTICAST
        byte pms[512];
        byte cr[32];
        byte sr[32];
        const byte suite[2] = {0, 0xfe};  /* WDM_WITH_NULL_SHA256 */

        XMEMSET(pms, 0x23, sizeof(pms));
        XMEMSET(cr, 0xA5, sizeof(cr));
        XMEMSET(sr, 0x5A, sizeof(sr));

        if (wolfSSL_set_secret(ssl, 1, pms, sizeof(pms), cr, sr, suite)
                != WOLFSSL_SUCCESS)
            err_sys("unable to set mcast secret");
#endif
    }

#ifndef NO_HANDSHAKE_DONE_CB
        wolfSSL_SetHsDoneCb(ssl, myHsDoneCb, NULL);
#endif
#ifdef HAVE_CRL
    if (!disableCRL) {
#ifdef HAVE_CRL_MONITOR
        crlFlags = CYASSL_CRL_MONITOR | CYASSL_CRL_START_MON;
#endif
        if (CyaSSL_EnableCRL(ssl, 0) != WOLFSSL_SUCCESS)
            err_sys_ex(runWithErrors, "unable to enable CRL");
        if (CyaSSL_LoadCRL(ssl, crlPemDir, WOLFSSL_FILETYPE_PEM, crlFlags)
                                                                 != WOLFSSL_SUCCESS)
            err_sys_ex(runWithErrors, "unable to load CRL");
        if (CyaSSL_SetCRL_Cb(ssl, CRL_CallBack) != WOLFSSL_SUCCESS)
            err_sys_ex(runWithErrors, "unable to set CRL callback url");
    }
#endif
#ifdef HAVE_OCSP
        if (useOcsp) {
            if (ocspUrl != NULL) {
                CyaSSL_CTX_SetOCSP_OverrideURL(ctx, ocspUrl);
                CyaSSL_CTX_EnableOCSP(ctx, CYASSL_OCSP_NO_NONCE
                                                        | CYASSL_OCSP_URL_OVERRIDE);
            }
            else
                CyaSSL_CTX_EnableOCSP(ctx, CYASSL_OCSP_NO_NONCE);
        }
#endif
#if defined(HAVE_CERTIFICATE_STATUS_REQUEST) \
 || defined(HAVE_CERTIFICATE_STATUS_REQUEST_V2)
        if (wolfSSL_CTX_EnableOCSPStapling(ctx) != WOLFSSL_SUCCESS)
            err_sys_ex(runWithErrors, "can't enable OCSP Stapling Certificate Manager");
        if (SSL_CTX_load_verify_locations(ctx, "certs/ocsp/intermediate1-ca-cert.pem", 0) != WOLFSSL_SUCCESS)
            err_sys_ex(runWithErrors, "can't load ca file, Please run from wolfSSL home dir");
        if (SSL_CTX_load_verify_locations(ctx, "certs/ocsp/intermediate2-ca-cert.pem", 0) != WOLFSSL_SUCCESS)
            err_sys_ex(runWithErrors, "can't load ca file, Please run from wolfSSL home dir");
        if (SSL_CTX_load_verify_locations(ctx, "certs/ocsp/intermediate3-ca-cert.pem", 0) != WOLFSSL_SUCCESS)
            err_sys_ex(runWithErrors, "can't load ca file, Please run from wolfSSL home dir");
#endif

#ifdef HAVE_PK_CALLBACKS
        if (pkCallbacks)
            SetupPkCallbackContexts(ssl, &pkCbInfo);
#endif

    #ifdef WOLFSSL_TLS13
        if (version >= 4) {
            WOLFSSL_START(WC_FUNC_CLIENT_KEY_EXCHANGE_DO);
            if (onlyKeyShare == 2) {
                if (useX25519 == 1) {
        #ifdef HAVE_CURVE25519
                    int groups[1] = { WOLFSSL_ECC_X25519 };

                    if (wolfSSL_UseKeyShare(ssl, WOLFSSL_ECC_X25519)
                                                           != WOLFSSL_SUCCESS) {
                        err_sys("unable to use curve x25519");
                    }
                    if (wolfSSL_set_groups(ssl, groups, 1) != WOLFSSL_SUCCESS) {
                        err_sys("unable to set groups: x25519");
                    }
        #endif
                }
                else
                {
        #ifdef HAVE_ECC
            #if defined(HAVE_ECC256) || defined(HAVE_ALL_CURVES)
                    int groups[1] = { WOLFSSL_ECC_SECP256R1 };

                    if (wolfSSL_UseKeyShare(ssl, WOLFSSL_ECC_SECP256R1)
                                                           != WOLFSSL_SUCCESS) {
                        err_sys("unable to use curve secp256r1");
                    }
                    if (wolfSSL_set_groups(ssl, groups, 1) != WOLFSSL_SUCCESS) {
                        err_sys("unable to set groups: secp256r1");
                    }
            #endif
        #endif
                }
            }
            else if (onlyKeyShare == 1) {
        #ifdef HAVE_FFDHE_2048
                int groups[1] = { WOLFSSL_FFDHE_2048 };

                if (wolfSSL_UseKeyShare(ssl, WOLFSSL_FFDHE_2048)
                                                           != WOLFSSL_SUCCESS) {
                    err_sys("unable to use DH 2048-bit parameters");
                }
                if (wolfSSL_set_groups(ssl, groups, 1) != WOLFSSL_SUCCESS) {
                    err_sys("unable to set groups: DH 2048-bit");
                }
        #endif
            }
            WOLFSSL_END(WC_FUNC_CLIENT_KEY_EXCHANGE_DO);
        }
    #endif


        /* do accept */
        readySignal = ((func_args*)args)->signal;
        if (readySignal) {
            readySignal->srfName = serverReadyFile;
        }
        tcp_accept(&sockfd, &clientfd, (func_args*)args, port, useAnyAddr,
                       dtlsUDP, dtlsSCTP, serverReadyFile ? 1 : 0, doListen);
        doListen = 0; /* Don't listen next time */

        if (SSL_set_fd(ssl, clientfd) != WOLFSSL_SUCCESS) {
            err_sys_ex(runWithErrors, "error in setting fd");
        }

#ifdef HAVE_ALPN
        if (alpnList != NULL) {
            printf("ALPN accepted protocols list : %s\n", alpnList);
            wolfSSL_UseALPN(ssl, alpnList, (word32)XSTRLEN(alpnList), alpn_opt);
        }
#endif

#ifdef WOLFSSL_DTLS
        if (doDTLS && dtlsUDP) {
            SOCKADDR_IN_T cliaddr;
            byte          b[1500];
            int           n;
            socklen_t     len = sizeof(cliaddr);

            /* For DTLS, peek at the next datagram so we can get the client's
             * address and set it into the ssl object later to generate the
             * cookie. */
            n = (int)recvfrom(sockfd, (char*)b, sizeof(b), MSG_PEEK,
                              (struct sockaddr*)&cliaddr, &len);
            if (n <= 0)
                err_sys_ex(runWithErrors, "recvfrom failed");

            wolfSSL_dtls_set_peer(ssl, &cliaddr, len);
        }
#endif
        if ((usePsk == 0 || usePskPlus) || useAnon == 1 || cipherList != NULL
                                                               || needDH == 1) {
            #if !defined(NO_FILESYSTEM) && !defined(NO_DH) && !defined(NO_ASN)
                CyaSSL_SetTmpDH_file(ssl, ourDhParam, WOLFSSL_FILETYPE_PEM);
            #elif !defined(NO_DH)
                SetDH(ssl);  /* repick suites with DHE, higher priority than PSK */
            #endif
        }

#ifndef CYASSL_CALLBACKS
        if (nonBlocking) {
#ifdef WOLFSSL_DTLS
            if (doDTLS) {
                wolfSSL_dtls_set_using_nonblock(ssl, 1);
            }
#endif
            tcp_set_nonblocking(&clientfd);
        }
#endif

#ifndef CYASSL_CALLBACKS
        if (nonBlocking) {
            ret = NonBlockingSSL_Accept(ssl);
        }
        else {
    #ifdef WOLFSSL_EARLY_DATA
            if (earlyData) {
                do {
                    int len;
                    err = 0; /* reset error */
                    ret = wolfSSL_read_early_data(ssl, input, sizeof(input)-1,
                                                                          &len);
                    if (ret != WOLFSSL_SUCCESS) {
                        err = SSL_get_error(ssl, 0);
                    #ifdef WOLFSSL_ASYNC_CRYPT
                        if (err == WC_PENDING_E) {
                            ret = wolfSSL_AsyncPoll(ssl, WOLF_POLL_FLAG_CHECK_HW);
                            if (ret < 0) break;
                        }
                    #endif
                    }
                    if (ret > 0) {
                        input[ret] = 0; /* null terminate message */
                        printf("Early Data Client message: %s\n", input);
                    }
                } while (err == WC_PENDING_E || ret > 0);
            }
    #endif
            do {
                err = 0; /* reset error */
                ret = SSL_accept(ssl);
                if (ret != WOLFSSL_SUCCESS) {
                    err = SSL_get_error(ssl, 0);
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
        ret = NonBlockingSSL_Accept(ssl);
#endif
        if (ret != WOLFSSL_SUCCESS) {
            err = SSL_get_error(ssl, 0);
            printf("SSL_accept error %d, %s\n", err,
                                                ERR_error_string(err, buffer));
            /* cleanup */
            SSL_free(ssl);
            SSL_CTX_free(ctx);
            CloseSocket(clientfd);
            CloseSocket(sockfd);

            if (!exitWithRet)
                err_sys_ex(runWithErrors, "SSL_accept failed");

            ((func_args*)args)->return_code = err;
            goto exit;
        }

        showPeer(ssl);
        if (SSL_state(ssl) != 0) {
            err_sys_ex(runWithErrors, "SSL in error state");
        }

#ifdef OPENSSL_EXTRA
    {
        byte*  rnd;
        byte*  pt;
        size_t size;

        /* get size of buffer then print */
        size = wolfSSL_get_server_random(NULL, NULL, 0);
        if (size == 0) {
            err_sys_ex(runWithErrors, "error getting server random buffer size");
        }

        rnd = (byte*)XMALLOC(size, NULL, DYNAMIC_TYPE_TMP_BUFFER);
        if (rnd == NULL) {
            err_sys_ex(runWithErrors, "error creating server random buffer");
        }

        size = wolfSSL_get_server_random(ssl, rnd, size);
        if (size == 0) {
            XFREE(rnd, NULL, DYNAMIC_TYPE_TMP_BUFFER);
            err_sys_ex(runWithErrors, "error getting server random buffer");
        }

        printf("Server Random : ");
        pt = rnd;
        if (pt != NULL) {
            for (pt = rnd; pt < rnd + size; pt++) printf("%02X", *pt);
            printf("\n");
        } else {
            err_sys_ex(runWithErrors, "error: attempted to dereference null "
                                                                   "pointer");
        }
        XFREE(rnd, NULL, DYNAMIC_TYPE_TMP_BUFFER);
    }
#endif

#ifdef HAVE_ALPN
        if (alpnList != NULL) {
            char *protocol_name = NULL, *list = NULL;
            word16 protocol_nameSz = 0, listSz = 0;

            err = wolfSSL_ALPN_GetProtocol(ssl, &protocol_name, &protocol_nameSz);
            if (err == WOLFSSL_SUCCESS)
                printf("Sent ALPN protocol : %s (%d)\n",
                       protocol_name, protocol_nameSz);
            else if (err == WOLFSSL_ALPN_NOT_FOUND)
                printf("No ALPN response sent (no match)\n");
            else
                printf("Getting ALPN protocol name failed\n");

            err = wolfSSL_ALPN_GetPeerProtocol(ssl, &list, &listSz);
            if (err == WOLFSSL_SUCCESS)
                printf("List of protocol names sent by Client: %s (%d)\n",
                       list, listSz);
            else
                printf("Get list of client's protocol name failed\n");

            free(list);
        }
#endif

#if !defined(NO_FILESYSTEM) && !defined(NO_CERTS)
    #if defined(WOLFSSL_TLS13) && defined(WOLFSSL_POST_HANDSHAKE_AUTH)
        if (postHandAuth) {
            SSL_set_verify(ssl, WOLFSSL_VERIFY_PEER |
                                ((usePskPlus) ? WOLFSSL_VERIFY_FAIL_EXCEPT_PSK :
                                WOLFSSL_VERIFY_FAIL_IF_NO_PEER_CERT), 0);
            if (SSL_CTX_load_verify_locations(ctx, verifyCert, 0)
                                                           != WOLFSSL_SUCCESS) {
                err_sys_ex(runWithErrors, "can't load ca file, Please run from "
                                          "wolfSSL home dir");
            }
            #ifdef WOLFSSL_TRUST_PEER_CERT
            if (trustCert) {
                if ((ret = wolfSSL_trust_peer_cert(ssl, trustCert,
                                    WOLFSSL_FILETYPE_PEM)) != WOLFSSL_SUCCESS) {
                    err_sys_ex(runWithErrors, "can't load trusted peer cert "
                                              "file");
                }
            }
            #endif /* WOLFSSL_TRUST_PEER_CERT */
       }
    #endif
#endif

        if (echoData == 0 && throughput == 0) {
            ServerRead(ssl, input, sizeof(input)-1);
            err = SSL_get_error(ssl, 0);
        }

        if (err != WOLFSSL_ERROR_ZERO_RETURN && echoData == 0 &&
                                                              throughput == 0) {
            const char* write_msg;
            int write_msg_sz;

#ifdef WOLFSSL_TLS13
            if (updateKeysIVs)
                wolfSSL_update_keys(ssl);
#endif
#if defined(WOLFSSL_TLS13) && defined(WOLFSSL_POST_HANDSHAKE_AUTH)
            if (postHandAuth)
                wolfSSL_request_certificate(ssl);
#endif

            /* Write data */
            if (!useWebServerMsg) {
                write_msg = msg;
                write_msg_sz = sizeof(msg);
            }
            else {
                write_msg = webServerMsg;
                write_msg_sz = sizeof(webServerMsg);
            }
            ServerWrite(ssl, write_msg, write_msg_sz);

#ifdef WOLFSSL_TLS13
            if (updateKeysIVs || postHandAuth)
                ServerRead(ssl, input, sizeof(input)-1);
#endif
        }
        else {
            ServerEchoData(ssl, clientfd, echoData, block, throughput);
        }

#if defined(WOLFSSL_MDK_SHELL) && defined(HAVE_MDK_RTX)
        os_dly_wait(500) ;
#elif defined (CYASSL_TIRTOS)
        Task_yield();
#endif

        if (dtlsUDP == 0) {
            ret = SSL_shutdown(ssl);
            if (wc_shutdown && ret == WOLFSSL_SHUTDOWN_NOT_DONE)
                SSL_shutdown(ssl);    /* bidirectional shutdown */
        }
        /* display collected statistics */
#ifdef WOLFSSL_STATIC_MEMORY
        if (wolfSSL_is_static_memory(ssl, &ssl_stats) != 1)
            err_sys_ex(runWithErrors, "static memory was not used with ssl");

        fprintf(stderr, "\nprint off SSL memory stats\n");
        fprintf(stderr, "*** This is memory state before wolfSSL_free is called\n");
        fprintf(stderr, "peak connection memory = %d\n", ssl_stats.peakMem);
        fprintf(stderr, "current memory in use  = %d\n", ssl_stats.curMem);
        fprintf(stderr, "peak connection allocs = %d\n", ssl_stats.peakAlloc);
        fprintf(stderr, "current connection allocs = %d\n",ssl_stats.curAlloc);
        fprintf(stderr, "total connection allocs   = %d\n",ssl_stats.totalAlloc);
        fprintf(stderr, "total connection frees    = %d\n\n", ssl_stats.totalFr);

#endif
        SSL_free(ssl);

        CloseSocket(clientfd);

        if (resume == 1 && resumeCount == 0) {
            resumeCount++;           /* only do one resume for testing */
            continue;
        }
        resumeCount = 0;

        cnt++;
        if (loops > 0 && --loops == 0) {
            break;  /* out of while loop, done with normal and resume option */
        }
    } /* while(1) */

    WOLFSSL_TIME(cnt);
    (void)cnt;

#if defined(HAVE_CERTIFICATE_STATUS_REQUEST) \
 || defined(HAVE_CERTIFICATE_STATUS_REQUEST_V2)
    wolfSSL_CTX_DisableOCSPStapling(ctx);
#endif

    CloseSocket(sockfd);
    SSL_CTX_free(ctx);

    ((func_args*)args)->return_code = 0;

exit:

#if defined(NO_MAIN_DRIVER) && defined(HAVE_ECC) && defined(FP_ECC) \
                            && defined(HAVE_THREAD_LS)
    ecc_fp_free();  /* free per thread cache */
#endif

#ifdef CYASSL_TIRTOS
    fdCloseSession(Task_self());
#endif

#if defined(HAVE_SESSION_TICKET) && defined(HAVE_CHACHA) && \
                                    defined(HAVE_POLY1305)
    TicketCleanup();
#endif

#ifdef WOLFSSL_ASYNC_CRYPT
    wolfAsync_DevClose(&devId);
#endif

    /* There are use cases  when these assignments are not read. To avoid
     * potential confusion those warnings have been handled here.
     */
    (void) ourKey;
    (void) verifyCert;
    (void) doCliCertCheck;
    (void) useNtruKey;
    (void) ourDhParam;
    (void) ourCert;
#ifndef CYASSL_TIRTOS
    return 0;
#endif
}

#endif /* !NO_WOLFSSL_SERVER */


/* so overall tests can pull in test function */
#ifndef NO_MAIN_DRIVER

    int main(int argc, char** argv)
    {
        func_args args;
        tcp_ready ready;


        StartTCP();

        args.argc = argc;
        args.argv = argv;
        args.signal = &ready;
        args.return_code = 0;
        InitTcpReady(&ready);

#if defined(DEBUG_CYASSL) && !defined(WOLFSSL_MDK_SHELL)
        CyaSSL_Debugging_ON();
#endif
        CyaSSL_Init();
        ChangeToWolfRoot();

#ifndef NO_WOLFSSL_SERVER
#ifdef HAVE_STACK_SIZE
        StackSizeCheck(&args, server_test);
#else
        server_test(&args);
#endif
#else
        printf("Server not compiled in!\n");
#endif

        CyaSSL_Cleanup();
        FreeTcpReady(&ready);

#ifdef HAVE_WNR
    if (wc_FreeNetRandom() < 0)
        err_sys_ex(runWithErrors, "Failed to free netRandom context");
#endif /* HAVE_WNR */

        return args.return_code;
    }

    int myoptind = 0;
    char* myoptarg = NULL;

#endif /* NO_MAIN_DRIVER */
