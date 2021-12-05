/*
* Home Accessory Architect OTA Installer
*
* Copyright 2020-2021 José Antonio Jiménez Campos (@RavenSystem)
*
*/

/*
 * Based on Life-Cycle-Manager (LCM) by HomeAccessoryKid (@HomeACcessoryKid), licensed under Apache License 2.0.
 * https://github.com/HomeACcessoryKid/life-cycle-manager
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <lwip/api.h>
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "lwip/etharp.h"
#include <esp8266.h>

#include <wolfssl/ssl.h>
#include <wolfssl/wolfcrypt/types.h>
#include <wolfssl/wolfcrypt/logging.h>
#include <wolfssl/wolfcrypt/sha512.h>
#include <wolfssl/wolfcrypt/ecc.h>
#include <wolfssl/wolfcrypt/asn_public.h>
#include <ota.h>

#include <spiflash.h>
#include <sysparam.h>
#include <rboot-api.h>

#include "header.h"

#ifdef HAABOOT
    #define MAXFILESIZE         ((SPIFLASH_BASE_ADDR - BOOT1SECTOR) - 16)
#else
    #define MAXFILESIZE         ((BOOT1SECTOR - BOOT0SECTOR) - 16)
#endif  //HAABOOT

static ecc_key public_key;
static byte file_first_byte[] = { 0xff };
static const byte magic1[] = "HAP";
static WOLFSSL_CTX* ctx = NULL;;
static char last_host[HOST_LEN];
static char last_location[RECV_BUF_LEN];

#ifdef DEBUG_WOLFSSL    
void MyLoggingCallback(const int logLevel, const char* const logMessage) {
    /*custom logging function*/
    INFO("loglevel: %d - %s", logLevel, logMessage);
}
#endif

static char *strstr_lc(char *full_string, const char *search) {
    const size_t search_len = strlen(search);
    for (size_t i = 0; i <= strlen(full_string) - search_len; i++) {
        if (tolower((unsigned char) full_string[i]) == tolower((unsigned char) search[0])) {
            for (size_t j = 0; j < search_len; j++) {
                if (tolower((unsigned char) full_string[i + j]) != tolower((unsigned char) search[j])) {
                    break;
                }
                
                if (j == search_len - 1) {
                    return full_string + i;
                }
            }
        }
    }
    
    return NULL;
}

static void ota_get_host(const char* repo) {
    memset(last_host, 0, HOST_LEN);
    const char *found = strchr(repo, '/');

    if (found) {
        memcpy(last_host, repo, found - repo);
    } else {
        memcpy(last_host, repo, strlen(repo));
    }
}

static void ota_get_location(const char* repo) {
    memset(last_location, 0, RECV_BUF_LEN);
    const char *found = strchr(repo, '/');

    if (found) {
        memcpy(last_location, found + 1, strlen(found) - 1);
    } else {
        last_location[0] = 0;
    }
}

void ota_init(char* repo, const bool is_ssl) {
    INFO("INIT");

    //ip_addr_t target_ip;
    
    //rboot setup
    rboot_config conf;
    conf = rboot_get_config();
    if (conf.count != 2 || conf.roms[0] != BOOT0SECTOR || conf.roms[1] != BOOT1SECTOR || conf.current_rom != 0) {
        conf.count = 2;
        conf.roms[0] = BOOT0SECTOR;
        conf.roms[1] = BOOT1SECTOR;
        conf.current_rom = 0;
        rboot_set_config(&conf);
    }
    
    if (is_ssl) {
#ifdef DEBUG_WOLFSSL
        if (wolfSSL_SetLoggingCb(MyLoggingCallback)) {
            ERROR("Setting debug callback");
        }
#endif
        wolfSSL_Init();

        //ctx = wolfSSL_CTX_new(wolfTLSv1_2_client_method());
        
        ctx = wolfSSL_CTX_new(wolfSSLv23_client_method());
        //wolfSSL_CTX_SetMinVersion(ctx, WOLFSSL_TLSV1_2);
        
        wolfSSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
    }
    
    word32 idx = 0;
    wc_ecc_init(&public_key);
    
    // Public key to verify signatures and avoid malware
    const byte raw_public_key[] = {
        0x30, 0x76, 0x30, 0x10, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce,
        0x3d, 0x02, 0x01, 0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x22,
        0x03, 0x62, 0x00, 0x04, 0x98, 0xe0, 0x54, 0xc4, 0x9b, 0x8a,
        0x41, 0x94, 0x76, 0xd1, 0x7f, 0xfd, 0xdf, 0x7b, 0xc4, 0xcc,
        0x8f, 0x97, 0x37, 0x89, 0x31, 0xd5, 0x17, 0x99, 0xe8, 0x0f,
        0x94, 0x3a, 0x85, 0x21, 0x09, 0xc6, 0xaa, 0xee, 0xb0, 0xee,
        0x58, 0x29, 0xb1, 0x14, 0x6d, 0x8f, 0x37, 0xcd, 0x01, 0x23,
        0x2f, 0xbf, 0x75, 0x3b, 0x70, 0xc2, 0xb9, 0x3f, 0x30, 0x4d,
        0x88, 0xf4, 0xc6, 0x5f, 0x8c, 0x24, 0x8a, 0x02, 0xd4, 0xce,
        0x65, 0x64, 0x24, 0xc2, 0x6d, 0xd2, 0x2c, 0x11, 0x95, 0x08,
        0x00, 0x5d, 0x4d, 0x9a, 0x9f, 0x1d, 0xab, 0x91, 0xf4, 0x04,
        0x66, 0x30, 0x94, 0x56, 0x3b, 0x4c, 0xb7, 0xba, 0xdb, 0x22
    };
    
    wc_EccPublicKeyDecode(raw_public_key, &idx, &public_key, sizeof(raw_public_key));
}

static int ota_connect(char* host, uint16_t port, int *socket, WOLFSSL** ssl, const bool is_ssl) {
    printf("*** CONNECTION\nDNS..");
    int ret;
    const struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo* res;

    void print_error() {
        INFO("ERROR");
    }
    
    char port_s[6];
    memset(port_s, 0, 6);
    itoa(port, port_s, 10);
    ret = getaddrinfo(host, port_s, &hints, &res);
    if (ret) {
        freeaddrinfo(res);
        print_error();
        return -2;
    }
    
    printf("OK Socket..");
    *socket = socket(res->ai_family, res->ai_socktype, 0);
    if (*socket < 0) {
        freeaddrinfo(res);
        print_error();
        return -3;
    }

    printf("OK Connect..");
    ret = connect(*socket, res->ai_addr, res->ai_addrlen);
    if (ret) {
        freeaddrinfo(res);
        print_error();
        return -2;
    }
    printf("OK ");

    // SSL mode
    if (is_ssl) {
        printf("SSL..");
        *ssl = wolfSSL_new(ctx);
        
        if (!*ssl) {
            freeaddrinfo(res);
            print_error();
            return -2;
        }

        printf("OK ");

        //wolfSSL_Debugging_ON();
        
        ret = wolfSSL_set_fd(*ssl, *socket);
        printf("%s:%d..", host, port);
        if (ret != SSL_SUCCESS) {
            freeaddrinfo(res);
            ERROR("%i", ret);
            ret = wolfSSL_get_error(*ssl, ret);
            ERROR("SSL %d", ret);
            return -1;
        }
        
        //wolfSSL_Debugging_OFF();
        
        ret = wolfSSL_connect(*ssl);
        if (ret != SSL_SUCCESS) {
            freeaddrinfo(res);
            ERROR("Connect %i", ret);
            ret = wolfSSL_get_error(*ssl, ret);
            ERROR("SSL %d", ret);
            return -1;
        }
        INFO("OK");
    }
    
    freeaddrinfo(res);
    
    return 0;
}

static int ota_get_final_location(char* repo, char* file, uint16_t port, const bool is_ssl) {
    int ota_conn_result;
    int ret = 0;
    int error = 0;
    uint16_t slash;
    WOLFSSL* ssl;
    int socket;
    
    size_t buffer_len = 0;
    char* buffer = NULL;
    char* recv_buf = NULL;
    char* location = NULL;

    ota_get_host(repo);
    ota_get_location(repo);
    
    if (strlen(last_location) > 0) {
        strcat(strcat(last_location, "/"), file);
    } else {
        strcat(last_location, file);
    }
    
    uint8_t i = 0;
    while (i < MAX_302_JUMPS) {
        i++;
        INFO("*** FORWARDING: %s:%i/%s", last_host, port, last_location);
        
        ota_conn_result = ota_connect(last_host, port, &socket, &ssl, is_ssl);  //release socket and ssl when ready
        
        const struct timeval rcvtimeout = { 1, 250000 };
        setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &rcvtimeout, sizeof(rcvtimeout));
        
        recv_buf = malloc(RECV_BUF_LEN);
        memset(recv_buf, 0, RECV_BUF_LEN);
        
        strcat(strcat(strcat(strcat(strcat(strcat(strcpy(recv_buf,
                                            REQUESTHEAD),
                                            last_location),
                                            REQUESTTAIL),
                                            last_host),
                                            RANGE),
                                            "0-1"),
                                            CRLFCRLF);
        
        if (ota_conn_result == 0) {
            const uint16_t send_bytes = strlen(recv_buf);
            
            if (is_ssl) {
                ret = wolfSSL_write(ssl, recv_buf, send_bytes);
            } else {
                ret = lwip_write(socket, recv_buf, send_bytes);
            }
            
            printf("Write ");
            if (ret > 0) {
                INFO("OK");
                
                //wolfSSL_shutdown(ssl); //by shutting down the connection before even reading, we reduce the payload to the minimum
                bool all_ok = false;
                
                do {
                    memset(recv_buf, 0, RECV_BUF_LEN);
                    
                    if (is_ssl) {
                        ret = wolfSSL_read(ssl, recv_buf, RECV_BUF_LEN - 1);
                    } else {
                        ret = lwip_read(socket, recv_buf, RECV_BUF_LEN - 1);
                    }
                    
                    if (ret > 0) {
                        all_ok = true;
                        buffer_len += ret;
                        char* new_buffer = realloc(buffer, buffer_len + 1);
                        if (!new_buffer) {
                            ERROR("Buffer");
                            break;
                        }
                        buffer = new_buffer;
                        memcpy(buffer + buffer_len - ret, recv_buf, ret);
                    }
                } while (ret > 0 && buffer_len < HEADER_BUFFER_LEN);
                
                if (all_ok) {
                    ret = buffer_len;
                    buffer[ret] = 0;
                    
                    INFO("GOT %i Bytes:\n%s\n", ret, buffer);
                    
                    location = strstr_lc(buffer, "http/1.1 ");
                    if (location) {
                        location += 9; // Flush "HTTP/1.1 "
                        slash = atoi(location);
                        INFO("HTTP returns %d\n", slash);
                        if (slash == 200 || slash == 206) {
                            i = MAX_302_JUMPS;
                            
                        } else if (slash == 302) {
                            location = strstr_lc(buffer, "\nlocation:");
                            if (location) {
                                strchr(location, '\r')[0] = 0;
                                if (location[10] == ' ') {
                                    location++;
                                }
                                
                                location = strstr_lc(location , "//");
                                location += 2; // Flush "//"

                                ota_get_host(location);
                                ota_get_location(location);
                                
                            } else {
                                i = MAX_302_JUMPS;
                                error = -1;
                            }
                            
                        } else {
                            i = MAX_302_JUMPS;
                            error = -2;
                        }

                    } else {
                        i = MAX_302_JUMPS;
                        error = -3;
                    }

                } else {
                    ERROR("Read %i", ret);
                    if (is_ssl) {
                        ret = wolfSSL_get_error(ssl, ret);
                        ERROR("SSL %d", ret);
                    }
                }
            } else {
                ERROR("%i", ret);
                if (is_ssl) {
                    ret = wolfSSL_get_error(ssl, ret);
                    ERROR("SSL %d", ret);
                }
            }
        }
        
        free(recv_buf);
        
        buffer_len = 0;
        if (buffer) {
            free(buffer);
            buffer = NULL;
        }

        switch (ota_conn_result) {
            case  0:
            case -1:
                if (is_ssl) {
                    wolfSSL_free(ssl);
                }
            case -2:
                lwip_close(socket);
            case -3:
            default:
            ;
        }
    }
    
    if (error < 0) {
        return error;
    }
    
    if (ota_conn_result < 0) {
        return ota_conn_result;
    }
    
    return ret;
}

#ifndef HAABOOT
static void sign_check_client(const int set) {
    byte* sector = malloc(SPI_FLASH_SECTOR_SIZE);
    
    if (!spiflash_read(SPIFLASH_BASE_ADDR, sector, SPI_FLASH_SECTOR_SIZE)) {
        ERROR("Read sector");
        return;
    }
    
    void write_flash() {
        if (!spiflash_erase_sector(SPIFLASH_BASE_ADDR) ||
            !spiflash_write(SPIFLASH_BASE_ADDR, sector, SPI_FLASH_SECTOR_SIZE)) {
            ERROR("Writing flash");
        }
    }
    
    if (set == 0 && sector[2] != magic1[2]) {
        sector[2] = magic1[2];
        write_flash();
        INFO("End ..");
    } else if (set == 1 && sector[2] != magic1[1]) {
        sector[2] = magic1[1];
        write_flash();
        INFO("End...");
    }
    
    free(sector);
}
#endif  // HAABOOT

static int ota_get_file_ex(char* repo, char* file, int sector, byte* buffer, int bufsz, uint16_t port, const bool is_ssl, int* resume) {
    INFO("*** DOWNLOADING");
    
    int ota_conn_result, ret = 0;
    WOLFSSL* ssl;
    int socket;
    
    int recv_bytes = 0;
    int collected = 0;
    
    if (resume) {
        collected = *resume;
        INFO("Resuming %i", collected);
    }
    
    int length = collected + 1;
    int clength = 0;
    int last_collected;
    int writespace = 0;
    int left, header;

    if (sector == 0 && buffer == NULL) {
        return -5;      // Needs to be either a sector or a signature/version file
    }
    
    int connection_tries = 0;
    while ((ota_conn_result = ota_get_final_location(repo, file, port, is_ssl)) <= 0 && connection_tries < 5) {
        connection_tries++;
        ERROR("Tries %i", connection_tries);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    
    if (ota_conn_result <= 0) {
        ERROR("FINAL LOC");
        return -1;
    }
    
    INFO("*** FINAL LOC: %s:%i/%s\n", last_host, port, last_location);
    
    int new_connection() {
        int tries = 0;
        int result;
        while ((result = ota_connect(last_host, port, &socket, &ssl, is_ssl)) < 0 && tries < 3) {
            tries++;
            ERROR("Tries %i", tries);
            
            switch (result) {
                case 0:
                case -1:
                    if (is_ssl) {
                        wolfSSL_free(ssl);
                    }
                case -2:
                    lwip_close(socket);
                case -3:
                default:
                ;
            }
            
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
        
        INFO("");
        
        return result;
    }
    
    ota_conn_result = new_connection();
    
    if (ota_conn_result == 0) {
        char* location;
        char* recv_buf = malloc(RECV_BUF_LEN);
        
        connection_tries = 0;
        while (collected < length && connection_tries < 4) {
            last_collected = collected;
            
            snprintf(recv_buf, RECV_BUF_LEN, REQUESTHEAD"%s"REQUESTTAIL"%s"RANGE"%d-%d%s", last_location, last_host, collected, collected + 4095, CRLFCRLF);
            
            const uint16_t send_bytes = strlen(recv_buf);
            
            if (is_ssl) {
                ret = wolfSSL_write(ssl, recv_buf, send_bytes);
            } else {
                ret = lwip_write(socket, recv_buf, send_bytes);
            }
            
            recv_bytes = 0;
            
            if (ret > 0) {
                header = 1;
                memset(recv_buf, 0, RECV_BUF_LEN);
                //wolfSSL_Debugging_ON();
                do {
                    if (is_ssl) {
                        ret = wolfSSL_read(ssl, recv_buf, RECV_BUF_LEN - 1);
                    } else {
                        ret = lwip_read(socket, recv_buf, RECV_BUF_LEN - 1);
                    }

                    if (ret > 0) {
                        if (header) {
                            //INFO("--------\n%s\n-------- %d", recv_buf, ret);
                            // Parse Content-Length: xxxx
                            
                            void try_new_conn() {
                                INFO("--\n%s\n-- %d", recv_buf, ret);
                                free(recv_buf);
                                collected = last_collected;
                                connection_tries++;
                                if (is_ssl) {
                                    wolfSSL_free(ssl);
                                }
                                lwip_close(socket);
                                vTaskDelay(2000 / portTICK_PERIOD_MS);
                                ota_conn_result = new_connection();
                                recv_buf = malloc(RECV_BUF_LEN);
                            }
                            
                            location = strstr_lc(recv_buf, "\ncontent-length:");
                            if (!location) {
                                ERROR("No CL");
                                try_new_conn();
                                break;
                            }
                            strchr(location, '\r')[0] = 0;
                            location += 16; // Flush "Content-Length:"
                            clength = atoi(location);
                            location[strlen(location)] = '\r'; // In case the order changes
                            // Parse Content-Range: bytes xxxx-yyyy/zzzz
                            location = strstr_lc(recv_buf, "\ncontent-range:");
                            if (location) {
                                strchr(location,'\r')[0] = 0;
                                location += 15; // Flush "Content-Range: "
                                location = strstr_lc(recv_buf, "bytes ");
                                location += 6; // Flush "bytes "
                                
                                location = strstr_lc(location, "/");
                                location++; // Flush "/"
                                
                                length = atoi(location);
                                
                                location[strlen(location)] = '\r'; // Search the entire buffer again
                            } else if (buffer) {
                                length = clength;
                            } else {
                                ERROR("No CR");
                                try_new_conn();
                                break;
                            }
                            
                            location = strstr_lc(recv_buf, CRLFCRLF) + 4; // Go to end of header
                            if ((left = ret - (location - recv_buf))) {
                                header = 0; // We have body in the same IP packet as the header so we need to process it already
                                ret = left;
                                memmove(recv_buf, location, left); // Move this payload to the head of the recv_buf
                            }
                        }
                        
                        if (length > MAXFILESIZE) {
                            ERROR("TOO BIG %i/%i", length, MAXFILESIZE);
                            free(recv_buf);
                            return -10;
                        }
                        
                        if (!header) {
                            recv_bytes += ret;
                            if (sector) { // Write to flash
                                connection_tries = 0;
                                if (writespace < ret) {
                                    printf("Sector 0x%05X ", sector + collected);
                                    if (!spiflash_erase_sector(sector + collected)) {
                                        free(recv_buf);
                                        return -6; // Erase error
                                    }
                                    writespace += SPI_FLASH_SECTOR_SIZE;
                                }
                                if (collected) {
                                    if (!spiflash_write(sector + collected, (byte *)recv_buf, ret)) {
                                        free(recv_buf);
                                        return -7; // Write error
                                    }
                                } else { // At the very beginning, do not write the first byte yet but store it for later
                                    file_first_byte[0] = (byte)recv_buf[0];
                                    if (!spiflash_write(sector + 1, (byte *)recv_buf + 1, ret - 1)) {
                                        free(recv_buf);
                                        return -8; // Write error
                                    }
                                }
                                writespace -= ret;
                            } else { // Buffer
                                if (ret > bufsz) {
                                    free(recv_buf);
                                    return -9; // Too big
                                }
                                memcpy(buffer, recv_buf, ret);
                            }
                            collected += ret;
                        }
                        
                    } else {
                        ERROR("Read %i", ret);
                        if (is_ssl) {
                            ret = wolfSSL_get_error(ssl, ret);
                            ERROR("SSL %d", ret);
                        }
                        
                        if (!ret && collected < length) {
                            free(recv_buf);
                            if (is_ssl) {
                                wolfSSL_free(ssl);
                            }
                            lwip_close(socket);
                            vTaskDelay(2000 / portTICK_PERIOD_MS);
                            ota_conn_result = new_connection();
                            recv_buf = malloc(RECV_BUF_LEN);
                        }
                        
                        break;
                    }
                    
                    header = 0; // Move to header section itself
                } while (recv_bytes < clength);
                
                INFO("- %d Bytes", collected);
                
            } else {
                ERROR("Write %i", ret);
                if (is_ssl) {
                    ret = wolfSSL_get_error(ssl, ret);
                    ERROR("SSL %d", ret);
                }
                
                if (ret == -308) {
                    free(recv_buf);
                    if (is_ssl) {
                        wolfSSL_free(ssl);
                    }
                    lwip_close(socket);
                    vTaskDelay(2000 / portTICK_PERIOD_MS);
                    ota_conn_result = new_connection();
                    recv_buf = malloc(RECV_BUF_LEN);
                } else {
                    break;
                }
            }
        }
        
        free(recv_buf);
    }
    
    INFO("");
    
    switch (ota_conn_result) {
        case 0:
        case -1:
            if (is_ssl) {
                wolfSSL_free(ssl);
            }
        case -2:
            lwip_close(socket);
        case -3:
        default:
        ;
    }
    
    if (resume) {
        *resume = collected;
    }
    
    if (connection_tries >= 3) {
        return 1;
    }
    
    return 0;
}

int ota_get_file_part(char* repo, char* file, int sector, uint16_t port, const bool is_ssl, int* collected) {
    INFO(">>> File part from %s", repo);
    
    return ota_get_file_ex(repo, file, sector, NULL, 0, port, is_ssl, collected);
}

int ota_get_file(char* repo, char* file, int sector, uint16_t port, const bool is_ssl) {
    INFO(">>> File from %s", repo);
    
    return ota_get_file_ex(repo, file, sector, NULL, 0, port, is_ssl, NULL);
}

char* ota_get_version(char* repo, char* version_file, uint16_t port, const bool is_ssl) {
    INFO(">>> Version from %s", repo);

    byte* version = malloc(VERSIONFILESIZE + 1);
    memset(version, 0, VERSIONFILESIZE + 1);

    if (ota_get_file_ex(repo, version_file, 0, version, VERSIONFILESIZE, port, is_ssl, NULL) == 0) {
        INFO("*** VERSION of %s: %s", version_file, (char*) version);
    } else {
        free(version);
        version = NULL;
        ERROR("ERROR");
    }
    
    return (char*) version;
}

int ota_get_sign(char* repo, char* file, byte* signature, uint16_t port, bool is_ssl) {
    INFO(">>> Get sign");
    int ret;
    char* signame = malloc(strlen(file) + 5);
    strcpy(signame, file);
    strcat(signame, SIGNFILESUFIX);
    memset(signature, 0, SIGNSIZE);
    ret = ota_get_file_ex(repo, signame, 0, signature, SIGNSIZE, port, is_ssl, NULL);
    free(signame);
    
    return ret;
}

int ota_verify_sign(int start_sector, int filesize, byte* signature) {
    INFO(">>> Verifying");
    
    int bytes;
    byte hash[HASHSIZE];
    byte* buffer = malloc(1024);
    Sha384 sha;
    
    wc_InitSha384(&sha);

    for (bytes = 0; bytes < filesize - 1024; bytes += 1024) {
        if (!spiflash_read(start_sector + bytes, (byte*) buffer, 1024)) {
            ERROR("Reading flash");
            break;
        }
        
        if (bytes == 0) {
            buffer[0] = file_first_byte[0];
        }
        
        wc_Sha384Update(&sha, buffer, 1024);
    }

    if (!spiflash_read(start_sector + bytes, (byte*) buffer, filesize - bytes)) {
        ERROR("Reading flash");
    }
    
    wc_Sha384Update(&sha, buffer, filesize - bytes);
    
    free(buffer);
    
    wc_Sha384Final(&sha, hash);
    
    int verify = 0;
    wc_ecc_verify_hash(signature, SIGNSIZE, hash, HASHSIZE, &verify, &public_key);
    
    INFO(">>> Sign result: %s", verify == 1 ? "OK" : "ERROR");
    
#ifndef HAABOOT
    sign_check_client(verify);
#endif  // HAABOOT

    return verify - 1;
}

void ota_finalize_file(int sector) {
    INFO("*** FINISHING");
    
    if (!spiflash_write(sector, file_first_byte, 1))
        ERROR("Writing flash");
}

void ota_reboot() {
    INFO("*** REBOOTING");

    vTaskDelay(1000 / portTICK_PERIOD_MS);
    sdk_system_restart();
}
