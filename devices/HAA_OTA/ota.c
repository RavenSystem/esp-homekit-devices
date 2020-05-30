/*
* Home Accessory Architect OTA Update
*
* Copyright 2020 José Antonio Jiménez Campos (@RavenSystem)
*
*/

/*
 * Based on Life-Cycle-Manager (LCM) by HomeAccessoryKid (@HomeACcessoryKid), licensed under Apache License 2.0.
 * https://github.com/HomeACcessoryKid/life-cycle-manager
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <lwip/sockets.h>
#include <lwip/api.h>
#include <esp8266.h>
#include <wolfssl/ssl.h>
#include <wolfssl/wolfcrypt/types.h>
#include <wolfssl/wolfcrypt/logging.h>
#include <wolfssl/wolfcrypt/sha512.h>
#include <wolfssl/wolfcrypt/ecc.h>
#include <wolfssl/wolfcrypt/asn_public.h>
#include <ota.h>

#include <sntp.h>
#include <spiflash.h>
#include <sysparam.h>
#include <rboot-api.h>

#include "header.h"

// Public key to verify signatures
static const byte raw_public_key[] = {
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

static ecc_key public_key;
static byte file_first_byte[] = { 0xff };
static WOLFSSL_CTX* ctx;
static int local_port = 0;
static char last_host[HOST_LEN];
static char last_location[RECV_BUF_LEN];

#ifdef DEBUG_WOLFSSL    
void MyLoggingCallback(const int logLevel, const char* const logMessage) {
    /*custom logging function*/
    printf("loglevel: %d - %s\n", logLevel, logMessage);
}
#endif

static char *strstr_lc(const char *full_string, const char *search) {
    char *lc_string = strdup(full_string);

    unsigned char *ch = (unsigned char *) lc_string;
    while (*ch) {
        *ch = tolower(*ch);
        ch++;
    }
    
    char *found = strstr(lc_string, search);
    free(lc_string);
    
    if (found == NULL) {
        return NULL;
    }
    
    const int offset = (uint32_t) found - (uint32_t) lc_string;
    
    return (char *) ((uint32_t) full_string + offset);
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
    printf("INIT\n");

    ip_addr_t target_ip;
    
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
/*
        // Time support
        const char *servers[] = { SNTP_SERVERS };
        sntp_set_update_delay(24 * 60 * 60000);     // SNTP will request an update every 24 hour

        sntp_initialize(NULL);
        sntp_set_servers(servers, sizeof(servers) / sizeof(char*));     // Servers must be configured right after initialization
*/
        
#ifdef DEBUG_WOLFSSL
        if (wolfSSL_SetLoggingCb(MyLoggingCallback)) {
            printf("error setting debug callback\n");
        }
#endif
        
        wolfSSL_Init();

        ctx = wolfSSL_CTX_new(wolfTLSv1_2_client_method());
//        if (!ctx) {
//           error
//        }
       
        wolfSSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
    }
    
    printf("DNS check result  = ");
    
    ota_get_host(repo);
    if (netconn_gethostbyname(last_host, &target_ip)) {
        printf("ERROR\n");
        ota_reboot();
    }

    word32 idx = 0;
    wc_ecc_init(&public_key);
    wc_EccPublicKeyDecode(raw_public_key, &idx, &public_key, sizeof(raw_public_key));


    printf("OK\n");
}

static int ota_connect(char* host, uint16_t port, int *socket, WOLFSSL** ssl, const bool is_ssl) {
    printf("NEW CONNECTION LocalPort=");
    int ret;
    ip_addr_t target_ip;
    struct sockaddr_in sock_addr;
    unsigned char initial_port[2];
    WC_RNG rng;
    
    if (!local_port) {
        wc_RNG_GenerateBlock(&rng, initial_port, 2);
        local_port = (256 * initial_port[0] + initial_port[1]) | 0xc000;
    }
    
    printf("%04x DNS..", local_port);
    
    ret = netconn_gethostbyname(host, &target_ip);
    if (ret) {
        printf(FAILED);
        return -2;
    }
    printf("OK ");
    
    printf("IP addr: %d.%d.%d.%d ", (unsigned char) ((target_ip.addr & 0x000000ff) >> 0),
                                    (unsigned char) ((target_ip.addr & 0x0000ff00) >> 8),
                                    (unsigned char) ((target_ip.addr & 0x00ff0000) >> 16),
                                    (unsigned char) ((target_ip.addr & 0xff000000) >> 24));

    *socket = socket(AF_INET, SOCK_STREAM, 0);
    if (*socket < 0) {
        printf(FAILED);
        return -3;
    }

    printf("Local..");
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = 0;
    sock_addr.sin_port = htons(local_port++);
    if (local_port == 0x10000) {
        local_port = 0xc000;
    }
    
    ret = bind(*socket, (struct sockaddr*) &sock_addr, sizeof(sock_addr));
    if (ret) {
        printf(FAILED);
        return -2;
    }
    printf("OK ");

    printf("Remote..");
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = target_ip.addr;
    sock_addr.sin_port = htons(port);
    ret = connect(*socket, (struct sockaddr*)&sock_addr, sizeof(sock_addr));
    if (ret) {
        printf(FAILED);
        return -2;
    }
    printf("OK ");

    if (is_ssl) {   // SSL mode
        printf("SSL..");
        *ssl = wolfSSL_new(ctx);
        
        if (!*ssl) {
            printf(FAILED);
            return -2;
        }

        printf("OK ");

        // wolfSSL_Debugging_ON();
        wolfSSL_set_fd(*ssl, *socket);
        printf("set_fd ");

        ret = wolfSSL_check_domain_name(*ssl, host);
        //wolfSSL_Debugging_OFF();

        printf("to %s port %d..", host, port);
        ret = wolfSSL_connect(*ssl);
        if (ret != SSL_SUCCESS) {
            printf("failed, return [-0x%x]\n", -ret);
            ret = wolfSSL_get_error(*ssl, ret);
            printf("wolfSSL_send error = %d\n", ret);
            return -1;
        }
        printf("OK");
    }
    
    printf("\n");
    
    return 0;
}

static int ota_get_final_location(char* repo, char* file, uint16_t port, const bool is_ssl) {
    int retc;
    int ret = 0;
    uint16_t slash;
    WOLFSSL* ssl;
    int socket;
    char recv_buf[RECV_BUF_LEN];
    
    char* location;

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
        printf("Forwarding: %s/%s\n", last_host, last_location);
        memset(recv_buf, 0, RECV_BUF_LEN);
        strcat(strcat(strcat(strcat(strcat(strcat(strcpy(recv_buf,
                                            REQUESTHEAD),
                                            last_location),
                                            REQUESTTAIL),
                                            last_host),
                                            RANGE),
                                            "0-1"),
                                            CRLFCRLF);
        
        uint16_t send_bytes = strlen(recv_buf);
        printf("-----\n%s-----\n", recv_buf);

        retc = ota_connect(last_host, port, &socket, &ssl, is_ssl);  //release socket and ssl when ready
        
        if (!retc) {
            if (is_ssl) {
                ret = wolfSSL_write(ssl, recv_buf, send_bytes);
            } else {
                ret = lwip_write(socket, recv_buf, send_bytes);
            }

            if (ret > 0) {
                printf("sent OK\n");

                //wolfSSL_shutdown(ssl); //by shutting down the connection before even reading, we reduce the payload to the minimum
                
                if (is_ssl) {
                    ret = wolfSSL_read(ssl, recv_buf, RECV_BUF_LEN - 1);
                    //ret = wolfSSL_peek(ssl, recv_buf, RECV_BUF_LEN - 1);
                } else {
                    ret = lwip_read(socket, recv_buf, RECV_BUF_LEN - 1);
                }
                
                printf("ret = %i\n", ret);
                
                if (ret > 0) {
                    recv_buf[ret] = 0; // Error checking, e.g. not result = 206
                    printf("\n%s\n\n", recv_buf);
                    location = strstr_lc(recv_buf, "http/1.1 ");
                    if (location) {
                        location += 9; // Flush "HTTP/1.1 "
                        slash = atoi(location);
                        printf("HTTP returns %d\n\n", slash);
                        if (slash == 200 || slash == 206) {
                            i = MAX_302_JUMPS;
                            
                        } else if (slash == 302) {
                            location = strstr_lc(recv_buf, "\nlocation:");
                            if (location) {
                                strchr(location, '\r')[0] = 0;
                                if (location[10] == ' ') {
                                    location++;
                                }
                                
                                location = strstr(location , "//");
                                location += 2; // Flush: //

                                ota_get_host(location);
                                ota_get_location(location);
                                
                            } else {
                                i = MAX_302_JUMPS;
                                ret = -1;
                            }
                            
                        } else {
                            i = MAX_302_JUMPS;
                            ret = -2;
                        }

                    } else {
                        i = MAX_302_JUMPS;
                        ret = -3;
                    }

                } else {
                    printf("failed, return [-0x%x]\n", -ret);
                    if (is_ssl) {
                        ret = wolfSSL_get_error(ssl, ret);
                        printf("wolfSSL_send error = %d\n", ret);
                    }
                }
            } else {
                printf("failed, return [-0x%x]\n", -ret);
                if (is_ssl) {
                    ret = wolfSSL_get_error(ssl, ret);
                    printf("wolfSSL_send error = %d\n", ret);
                }
            }
        }

        switch (retc) {
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

    if (retc) {
        return retc;
    }

    return ret;
}

static int ota_get_file_ex(char* repo, char* file, int sector, byte* buffer, int bufsz, uint16_t port, const bool is_ssl) { //number of bytes
    printf("\nDOWNLOADING FILE\n\n");
    
    int retc, ret = 0;
    WOLFSSL* ssl;
    int socket;

    char recv_buf[RECV_BUF_LEN];
    int recv_bytes = 0;
    int send_bytes;     // = sizeof(send_data);
    int length = 1;
    int clength = 0;
    int collected = 0;
    int writespace = 0;
    int left, header;

    if (sector == 0 && buffer == NULL) {
        return -5;      // Needs to be either a sector or a signature/version file
    }
    
    if (ota_get_final_location(repo, file, port, is_ssl) <= 0) {
        printf("!!! SERVER ERROR\n");
        return -1;
    }
    
    printf("FINAL location: %s/%s\n\n", last_host, last_location);
    
    retc = ota_connect(last_host, port, &socket, &ssl, is_ssl);  //release socket and ssl when ready
    
    if (!retc) {
        const uint16_t getlinestart_len = 38 + strlen(last_location) + strlen(last_host);
        
        char *getlinestart = malloc(getlinestart_len);
        snprintf(getlinestart, getlinestart_len, REQUESTHEAD"%s"REQUESTTAIL"%s"RANGE,
                 last_location,
                 last_host);

        char *location;
        
        while (collected < length) {
            sprintf(recv_buf, "%s%d-%d%s", getlinestart, collected, collected + 4095, CRLFCRLF);
            
            send_bytes = strlen(recv_buf);

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
                            //printf("%s\n-------- %d\n", recv_buf, ret);
                            // Parse Content-Length: xxxx
                            location = strstr_lc(recv_buf, "\ncontent-length:");
                            if (!location) {
                                printf("\n!!! ERROR No content-length found\n\n");
                                length = 0;
                                break;
                            }
                            strchr(location, '\r')[0] = 0;
                            location += 16; // Flush Content-Length:
                            clength = atoi(location);
                            location[strlen(location)] = '\r'; // In case the order changes
                            // Parse Content-Range: bytes xxxx-yyyy/zzzz
                            location = strstr_lc(recv_buf, "\ncontent-range:");
                            if (location) {
                                strchr(location,'\r')[0] = 0;
                                location += 15; // Flush Content-Range:
                                location = strstr_lc(recv_buf, "bytes ");
                                location += 6; // bytes
                                
                                location = strstr(location, "/");
                                location++; // Flush /
                                
                                length = atoi(location);
                                
                                location[strlen(location)] = '\r'; // Search the entire buffer again
                            } else if (buffer) {
                                length = clength;
                            } else {
                                printf("\n!!! ERROR No content-range found\n\n");
                                length = 0;
                                break;
                            }
                            
                            location = strstr(recv_buf, CRLFCRLF) + 4; // Go to end of header
                            if ((left = ret - (location - recv_buf))) {
                                header = 0; // We have body in the same IP packet as the header so we need to process it already
                                ret = left;
                                memmove(recv_buf, location, left); // Move this payload to the head of the recv_buf
                            }
                        }
                        
                        if (!header) {
                            recv_bytes += ret;
                            if (sector) { // Write to flash
                                if (writespace < ret) {
                                    printf("Sector 0x%05x ", sector + collected);
                                    if (!spiflash_erase_sector(sector + collected)) return -6; // Erase error
                                    writespace += SECTORSIZE;
                                }
                                if (collected) {
                                    if (!spiflash_write(sector + collected, (byte *)recv_buf, ret)) return -7; // Write error
                                } else { // At the very beginning, do not write the first byte yet but store it for later
                                    file_first_byte[0] = (byte)recv_buf[0];
                                    if (!spiflash_write(sector + 1, (byte *)recv_buf + 1, ret - 1)) return -7; // Write error
                                }
                                writespace -= ret;
                            } else { // Buffer
                                if (ret > bufsz) return -8; // Too big
                                memcpy(buffer, recv_buf, ret);
                            }
                            collected += ret;
                        }
                        
                    } else {
                        if (ret && is_ssl) {
                            ret = wolfSSL_get_error(ssl, ret);
                            printf("! %d\n", ret);
                        }
                        
                        if (!ret && collected < length)
                            retc = ota_connect(last_host, port, &socket, &ssl, is_ssl);
                        
                        break;
                    }
                    
                    header = 0; // Move to header section itself
                } while (recv_bytes < clength);
                
                printf(" Downloaded %d Bytes\n", collected);
                
            } else {
                printf("failed, return [-0x%x]\n", -ret);
                
                if (is_ssl) {
                    ret = wolfSSL_get_error(ssl, ret);
                    printf("wolfSSL_send error = %d\n", ret);
                }
                
                if (ret == -308) {
                    retc = ota_connect(last_host, port, &socket, &ssl, is_ssl); //dangerous for eternal connecting? memory leak?
                } else {
                    break;
                }
            }
        }
        
        free(getlinestart);
    } else {
        printf(FAILED);
    }
    
    printf("\n");
    
    switch (retc) {
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

    return collected;
}

int ota_get_file(char* repo, char* file, int sector, uint16_t port, const bool is_ssl) {
    printf("Get file from %s\n", repo);
    
    return ota_get_file_ex(repo, file, sector, NULL, 0, port, is_ssl);
}

char* ota_get_version(char* repo, char* version_file, uint16_t port, const bool is_ssl) {
    printf("Get version from %s\n", repo);

    byte* version = malloc(VERSIONFILESIZE + 1);
    memset(version, 0, VERSIONFILESIZE + 1);

    if (ota_get_file_ex(repo, version_file, 0, version, VERSIONFILESIZE, port, is_ssl)) {
        printf("VERSION of %s: %s\n", version_file, (char*) version);
    } else {
        free(version);
        version = NULL;
        printf("ERROR\n");
    }
    
    return (char*) version;
}

int ota_get_sign(char* repo, char* file, byte* signature, uint16_t port, bool is_ssl) {
    printf("Get sign\n");
    int ret;
    char* signame = malloc(strlen(file) + 5);
    strcpy(signame, file);
    strcat(signame, SIGNFILESUFIX);
    memset(signature, 0, SIGNSIZE);
    ret = ota_get_file_ex(repo, signame, 0, signature, SIGNSIZE, port, is_ssl);
    free(signame);
    
    return ret;
}

int ota_verify_sign(int start_sector, int filesize, byte* signature) {
    printf("Verifying sign...\n");
    
    int bytes;
    byte hash[HASHSIZE];
    byte buffer[1024];
    Sha384 sha;
    
    wc_InitSha384(&sha);

    for (bytes = 0; bytes < filesize - 1024; bytes += 1024) {
        if (!spiflash_read(start_sector + bytes, (byte*) buffer, 1024)) {
            printf("! Reading flash\n");
            break;
        }
        
        if (bytes == 0) {
            buffer[0] = file_first_byte[0];
        }
        
        wc_Sha384Update(&sha, buffer, 1024);
    }

    if (!spiflash_read(start_sector + bytes, (byte*) buffer, filesize - bytes)) {
        printf("! Reading flash\n");
    }
    
    wc_Sha384Update(&sha, buffer, filesize - bytes);
    wc_Sha384Final(&sha, hash);
    
    int verify = 0;
    wc_ecc_verify_hash(signature, SIGNSIZE, hash, HASHSIZE, &verify, &public_key);
    
    printf("Sign result: %s (%d)\n", verify == 1 ? "OK" : "ERROR" , verify);

    return verify - 1;
}

void ota_finalize_file(int sector) {
    printf("Finalize file\n");

    if (!spiflash_write(sector, file_first_byte, 1))
        printf("! Writing flash\n");
}

void ota_reboot() {
    printf("\nRestarting...\n");

    vTaskDelay(1000 / portTICK_PERIOD_MS);
    sdk_system_restart();
}

