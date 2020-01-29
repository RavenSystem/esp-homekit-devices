/*
* Home Accessory Architect OTA Update
*
* Copyright 2020 José Antonio Jiménez Campos (@RavenSystem)
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

/*  (c) 2018-2019 HomeAccessoryKid */

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
#include <ota.h>

#include <sntp.h>
#include <spiflash.h>
#include <sysparam.h>
#include <rboot-api.h>

#include <udplogger.h>

static byte file_first_byte[] = {0xff};

WOLFSSL_CTX* ctx;

#ifdef DEBUG_WOLFSSL    
void MyLoggingCallback(const int logLevel, const char* const logMessage) {
    /*custom logging function*/
    UDPLGP("loglevel: %d - %s\n", logLevel, logMessage);
}
#endif

void ota_init() {
    UDPLGP("OTA INIT\n");

    ip_addr_t target_ip;
    int ret;
    
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
    
    //time support
    const char *servers[] = {SNTP_SERVERS};
	sntp_set_update_delay(24 * 60 * 60000); //SNTP will request an update every 24 hour

	sntp_initialize(NULL);
	sntp_set_servers(servers, sizeof(servers) / sizeof(char*)); //Servers must be configured right after initialization

#ifdef DEBUG_WOLFSSL    
    if (wolfSSL_SetLoggingCb(MyLoggingCallback)) UDPLGP("error setting debug callback\n");
#endif
    
    wolfSSL_Init();

    ctx = wolfSSL_CTX_new(wolfTLSv1_2_client_method());
//    if (!ctx) {
        //error
//    }
   
    wolfSSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
    
    UDPLGP("DNS ");
    
    ret = netconn_gethostbyname(HOST, &target_ip);
    while(ret) {
        UDPLGP("%d", ret);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        ret = netconn_gethostbyname(HOST, &target_ip);
    }

    UDPLGP("OK\n");
}

void ota_hash(int start_sector, int filesize, byte * hash, byte first_byte) {
    UDPLGP("Creating hash\n");
    
    int bytes;
    byte buffer[1024];
    Sha384 sha;
    
    wc_InitSha384(&sha);

    for (bytes=0; bytes<filesize - 1024; bytes+=1024) {
        if (!spiflash_read(start_sector+bytes, (byte *)buffer, 1024)) {
            UDPLGP("error reading flash\n");
            break;
        }
        if (!bytes && first_byte != 0xff)
            buffer[0] = first_byte;
        
        wc_Sha384Update(&sha, buffer, 1024);
    }

    if (!spiflash_read(start_sector+bytes, (byte *)buffer, filesize-bytes)) {
        UDPLGP("error reading flash\n");
    }
    
    if (!bytes && first_byte != 0xff)
        buffer[0] = first_byte;
    
    wc_Sha384Update(&sha, buffer, filesize-bytes);
    wc_Sha384Final(&sha, hash);
}

int ota_compare(char* newv, char* oldv) { //(if equal,0) (if newer,1) (if pre-release or older,-1)
    UDPLGP("Compare\n");
    char * dot;
    int valuen = 0, valueo = 0;
    char news[MAXVERSIONLEN], olds[MAXVERSIONLEN];
    char * new = news;
    char * old = olds;
    int result=0;
    
    if (strcmp(newv, oldv)) { //https://semver.org/#spec-item-11
        do {
            if (strchr(newv,'-')) {result=-1;break;} //we cannot handle versions with pre-release suffix notation (yet)
            //pre-release marker in github serves to identify those
            strncpy(new,newv,MAXVERSIONLEN-1);
            strncpy(old,oldv,MAXVERSIONLEN-1);
            if ((dot=strchr(new,'.'))) {dot[0]=0; valuen=atoi(new); new=dot+1;}
            if ((dot=strchr(old,'.'))) {dot[0]=0; valueo=atoi(old); old=dot+1;}
            printf("%d-%d,%s-%s\n",valuen,valueo,new,old);
            if (valuen>valueo) {result= 1;break;}
            if (valuen<valueo) {result=-1;break;}
            valuen=valueo=0;
            if ((dot=strchr(new,'.'))) {dot[0]=0; valuen=atoi(new); new=dot+1;}
            if ((dot=strchr(old,'.'))) {dot[0]=0; valueo=atoi(old); old=dot+1;}
            printf("%d-%d,%s-%s\n",valuen,valueo,new,old);
            if (valuen>valueo) {result= 1;break;}
            if (valuen<valueo) {result=-1;break;}
            valuen=atoi(new);
            valueo=atoi(old);
            printf("%d-%d\n",valuen,valueo);
            if (valuen>valueo) {result= 1;break;}
            if (valuen<valueo) {result=-1;break;}        
        } while(0);
    } //they are equal
    UDPLGP("%s with %s = %d\n", newv, oldv, result);
    return result;
}

int local_port = 0;
static int ota_connect(char* host, int port, int *socket, WOLFSSL** ssl) {
    UDPLGP("NEW CONNECTION LocalPort=");
    int ret;
    ip_addr_t target_ip;
    struct sockaddr_in sock_addr;
    unsigned char initial_port[2];
    WC_RNG rng;
    
    if (!local_port) {
        wc_RNG_GenerateBlock(&rng, initial_port, 2);
        local_port = (256 * initial_port[0] + initial_port[1]) | 0xc000;
    }
    UDPLGP("%04x DNS",local_port);
    ret = netconn_gethostbyname(host, &target_ip);
    while(ret) {
        printf("%d",ret);
        vTaskDelay(200);
        ret = netconn_gethostbyname(host, &target_ip);
    }
    UDPLGP(" IP: %d.%d.%d.%d ", (unsigned char)((target_ip.addr & 0x000000ff) >> 0),
                              (unsigned char)((target_ip.addr & 0x0000ff00) >> 8),
                              (unsigned char)((target_ip.addr & 0x00ff0000) >> 16),
                              (unsigned char)((target_ip.addr & 0xff000000) >> 24));

    *socket = socket(AF_INET, SOCK_STREAM, 0);
    if (*socket < 0) {
        UDPLGP(FAILED);
        return -3;
    }

    UDPLGP("local..");
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = 0;
    sock_addr.sin_port = htons(local_port++);
    if (local_port == 0x10000)
        local_port = 0xc000;
    
    ret = bind(*socket, (struct sockaddr*)&sock_addr, sizeof(sock_addr));
    if (ret) {
        UDPLGP(FAILED);
        return -2;
    }
    UDPLGP("OK ");

    UDPLGP("remote..");
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = target_ip.addr;
    sock_addr.sin_port = htons(port);
    ret = connect(*socket, (struct sockaddr*)&sock_addr, sizeof(sock_addr));
    if (ret) {
        UDPLGP(FAILED);
        return -2;
    }
    UDPLGP("OK ");

    UDPLGP("SSL..");
    *ssl = wolfSSL_new(ctx);
    if (!*ssl) {
        UDPLGP(FAILED);
        return -2;
    }
    UDPLGP("OK ");

//wolfSSL_Debugging_ON();
    wolfSSL_set_fd(*ssl, *socket);
    UDPLGP("set_fd ");

    ret = wolfSSL_check_domain_name(*ssl, host);
//wolfSSL_Debugging_OFF();

    UDPLGP("to %s port %d..", host, port);
    ret = wolfSSL_connect(*ssl);
    if (ret != SSL_SUCCESS) {
        UDPLGP("failed, return [-0x%x]\n", -ret);
        ret = wolfSSL_get_error(*ssl, ret);
        UDPLGP("wolfSSL_send error = %d\n", ret);
        return -1;
    }
    UDPLGP("OK\n");
    return 0;

}

int ota_load_user_app(char * *repo, char * *version, char * *file) {
    UDPLGP("HAA firmware\n");
    sysparam_status_t status;
    
    *repo = USERREPO;
    *file = USERFILE;
    
    char *value;
    status = sysparam_get_string("ota_version", &value);
    if (status == SYSPARAM_OK)
        *version = value;
    else
        return -1;

    UDPLGP("Repo=\'%s\' Version=\'%s\' File=\'%s\'\n", *repo, *version, *file);
    return 0;
}

int ota_get_file_ex(char * repo, char * version, char * file, int sector, byte * buffer, int bufsz);
char* ota_get_version(char * repo) {
    UDPLGP("Get version %s\n", repo);

    char* version = NULL;
    int retc, ret = 0;
    int httpcode;
    WOLFSSL* ssl;
    int socket;
    char* location;
    char recv_buf[RECV_BUF_LEN];
    int  send_bytes; //= sizeof(send_data);
    
    strcat(strcat(strcat(strcat(strcat(strcpy(recv_buf, \
        REQUESTHEAD), repo), "/releases/latest"), REQUESTTAIL), HOST), CRLFCRLF);
    send_bytes = strlen(recv_buf);
    //printf("%s\n",recv_buf);

    retc = ota_connect(HOST, HTTPS_PORT, &socket, &ssl);  //release socket and ssl when ready
    
    if (!retc) {
        UDPLGP("\n%s\n", recv_buf);
        ret = wolfSSL_write(ssl, recv_buf, send_bytes);
        if (ret > 0) {
            printf("sent OK\n");

            //wolfSSL_shutdown(ssl); //by shutting down the connection before even reading, we reduce the payload to the minimum
            ret = wolfSSL_peek(ssl, recv_buf, RECV_BUF_LEN - 1);
            if (ret > 0) {
                recv_buf[ret] = 0; //error checking

                location=strstr(recv_buf,"HTTP/1.1 ");
                strchr(location, ' ')[0] = 0;
                location += 9; //flush "HTTP/1.1 "
                httpcode = atoi(location);
                UDPLGP("HTTP returns %d for ", httpcode);
                if (httpcode != 302) {
                    wolfSSL_free(ssl);
                    lwip_close(socket);
                    return "404";
                }
                recv_buf[strlen(recv_buf)] = ' '; //for further headers

                location = strstr(recv_buf, "Location: ");
                strchr(location,'\r')[0] = 0;

                location = strstr(location, "tag/");
                version = malloc(strlen(location + 4));
                strcpy(version, location + 4);
                printf("%s@version:\"%s\" according to latest release\n", repo, version);
            } else {
                UDPLGP("failed, return [-0x%x]\n", -ret);
                ret = wolfSSL_get_error(ssl, ret);
                UDPLGP("wolfSSL_send error = %d\n", ret);
            }
        } else {
            UDPLGP("failed, return [-0x%x]\n", -ret);
            ret = wolfSSL_get_error(ssl, ret);
            UDPLGP("wolfSSL_send error = %d\n", ret);
        }
    }
    
    switch (retc) {
        case  0:
        case -1:
            wolfSSL_free(ssl);
        case -2:
            lwip_close(socket);
        case -3:
        default:
        ;
    }

    if (ota_boot() && ota_compare(version, OTAVERSION) < 0) { //this acts when setting up a new version
        free(version);
        version = malloc(strlen(OTAVERSION) + 1);
        strcpy(version, OTAVERSION);
    }
    
    UDPLGP("%s@version:\"%s\"\n", repo, version);
    return version;
}

int ota_get_file_ex(char * repo, char * version, char * file, int sector, byte * buffer, int bufsz) { //number of bytes
    UDPLGP("DOWNLOADING\n");
    
    int retc, ret = 0, slash;
    WOLFSSL* ssl;
    int socket;

    char* location;
    char recv_buf[RECV_BUF_LEN];
    int recv_bytes = 0;
    int send_bytes; //= sizeof(send_data);
    int length = 1;
    int clength;
    int collected = 0;
    int writespace = 0;
    int header;
    
    if (sector == 0 && buffer == NULL)
        return -5; //needs to be either a sector or a signature
    
    strcat(strcat(strcat(strcat(strcat(strcat(strcat(strcat(strcpy(recv_buf, \
        REQUESTHEAD), repo), "/releases/download/"), version), "/"), file), REQUESTTAIL), HOST), CRLFCRLF);
    send_bytes = strlen(recv_buf);
    UDPLGP("%s", recv_buf);

    retc = ota_connect(HOST, HTTPS_PORT, &socket, &ssl);  //release socket and ssl when ready
    
    if (!retc) {
        ret = wolfSSL_write(ssl, recv_buf, send_bytes);
        if (ret > 0) {
            UDPLGP("sent OK\n");

            //wolfSSL_shutdown(ssl); //by shutting down the connection before even reading, we reduce the payload to the minimum
            ret = wolfSSL_peek(ssl, recv_buf, RECV_BUF_LEN - 1);
            if (ret > 0) {
                recv_buf[ret] = 0; //error checking, e.g. not result=206
                printf("\n%s\n\n", recv_buf);
                location = strstr(recv_buf, "HTTP/1.1 ");
                strchr(location,' ')[0] = 0;
                location += 9; //flush "HTTP/1.1 "
                slash = atoi(location);
                UDPLGP("HTTP returns %d\n", slash);
                if (slash != 302) {
                    wolfSSL_free(ssl);
                    lwip_close(socket);
                    return -1;
                }
                recv_buf[strlen(recv_buf)] = ' '; //for further headers
                location = strstr(recv_buf, "Location: ");
                strchr(location, '\r')[0] = 0;
                location += 18; //flush Location: https://

            } else {
                UDPLGP("failed, return [-0x%x]\n", -ret);
                ret = wolfSSL_get_error(ssl, ret);
                UDPLGP("wolfSSL_send error = %d\n", ret);
            }
        } else {
            UDPLGP("failed, return [-0x%x]\n", -ret);
            ret  =wolfSSL_get_error(ssl, ret);
            UDPLGP("wolfSSL_send error = %d\n", ret);
        }
    }
    switch (retc) {
        case  0:
        case -1:
        wolfSSL_free(ssl);
        case -2:
        lwip_close(socket);
        case -3:
        default:
        ;
    }

    if (retc)
        return retc;
    
    if (ret <= 0)
        return ret;
    
    //process the Location
    strcat(location, REQUESTTAIL);
    slash = strchr(location, '/') - location;
    location[slash] = 0; //cut behind the hostname
    char * host2 = malloc(strlen(location));
    strcpy(host2, location);


    retc = ota_connect(host2, HTTPS_PORT, &socket, &ssl);  //release socket and ssl when ready

    strcat(strcat(location + slash + 1, host2), RANGE); //append hostname and range to URI
    location += slash - 4;
    memcpy(location, REQUESTHEAD, 5);
    char * getlinestart = malloc(strlen(location));
    strcpy(getlinestart, location);

    while (collected < length) {
        sprintf(recv_buf, "%s%d-%d%s", getlinestart, collected, collected + 4095, CRLFCRLF);
        send_bytes=strlen(recv_buf);

        //printf("Send request: ");
        ret = wolfSSL_write(ssl, recv_buf, send_bytes);
        recv_bytes=0;
        if (ret > 0) {
            //printf("OK\n");

            header=1;
            memset(recv_buf,0,RECV_BUF_LEN);
            //wolfSSL_Debugging_ON();
            do {
                ret = wolfSSL_read(ssl, recv_buf, RECV_BUF_LEN - 1);
                if (ret > 0) {
                    if (header) {
                        //printf("%s\n-------- %d\n", recv_buf, ret);
                        //parse Content-Length: xxxx
                        location = strstr(recv_buf, "Content-Length: ");
                        strchr(location, '\r')[0] = 0;
                        location += 16; //flush Content-Length: //
                        clength = atoi(location);
                        location[strlen(location)] = '\r'; //in case the order changes
                        //parse Content-Range: bytes xxxx-yyyy/zzzz
                        location = strstr(recv_buf, "Content-Range: bytes ");
                        strchr(location, '\r')[0] = 0;
                        location += 21; //flush Content-Range: bytes //
                        location = strstr(location,"/"); location++; //flush /
                        length = atoi(location);
                        //verify if last bytes are crlfcrlf else header=1
                    } else {
                        recv_bytes += ret;
                        if (sector) { //write to flash
                            if (writespace < ret) {
                                UDPLGP("Sector 0x%05x ", sector + collected);
                                if (!spiflash_erase_sector(sector + collected)) return -6; //erase error
                                writespace += SECTORSIZE;
                            }
                            if (collected) {
                                if (!spiflash_write(sector + collected, (byte *)recv_buf, ret)) return -7; //write error
                            } else { //at the very beginning, do not write the first byte yet but store it for later
                                file_first_byte[0]=(byte)recv_buf[0];
                                if (!spiflash_write(sector + 1, (byte *)recv_buf + 1, ret - 1)) return -7; //write error
                            }
                            writespace -= ret;
                        } else { //buffer
                            if (ret > bufsz) return -8; //too big
                            memcpy(buffer, recv_buf, ret);
                        }
                        collected += ret;
                    }
                } else {
                    if (ret) {
                        ret = wolfSSL_get_error(ssl, ret);
                        UDPLGP("! %d\n", ret);
                    }
                    
                    if (!ret && collected < length)
                        retc = ota_connect(host2, HTTPS_PORT, &socket, &ssl); //memory leak?
                    
                    break;
                }
                header = 0; //move to header section itself
            } while (recv_bytes < clength);
            
            printf(" Downloaded %d Bytes\n", collected);
            UDPLOG(" Downloaded %d Bytes                \r", collected);
            
        } else {
            printf("failed, return [-0x%x]\n", -ret);
            ret = wolfSSL_get_error(ssl,ret);
            printf("wolfSSL_send error = %d\n", ret);
            if (ret == -308) {
                retc = ota_connect(host2, HTTPS_PORT, &socket, &ssl); //dangerous for eternal connecting? memory leak?
            } else {
                break;
            }
        }
    }
    
    UDPLOG("\n");
    switch (retc) {
        case  0:
        case -1:
        wolfSSL_free(ssl);
        case -2:
        lwip_close(socket);
        case -3:
        default:
        ;
    }
    free(host2);
    free(getlinestart);
    return collected;
}

void ota_finalize_file(int sector) {
    UDPLGP("Finalize file\n");

    if (!spiflash_write(sector, file_first_byte, 1))
        UDPLGP("! Writing flash\n");
}

int ota_get_file(char * repo, char * version, char * file, int sector) {
    UDPLGP("Get file\n");
    return ota_get_file_ex(repo, version, file, sector, NULL, 0);
}

int ota_get_hash(char * repo, char * version, char * file, signature_t* signature) {
    UDPLGP("Get hash\n");
    int ret;
    byte buffer[HASHSIZE + 4 + SIGNSIZE];
    char * signame = malloc(strlen(file) + 5);
    strcpy(signame, file);
    strcat(signame, ".sig");
    memset(signature->hash, 0, HASHSIZE);
    memset(signature->sign, 0, SIGNSIZE);
    ret = ota_get_file_ex(repo, version, signame, 0, buffer, HASHSIZE + 4 + SIGNSIZE);
    free(signame);
    
    if (ret < 0)
        return ret;
    
    memcpy(signature->hash, buffer, HASHSIZE);
    signature->size = ((buffer[HASHSIZE] * 256 + buffer[HASHSIZE + 1]) * 256 + buffer[HASHSIZE + 2]) * 256 + buffer[HASHSIZE + 3];
    if (ret > HASHSIZE + 4) memcpy(signature->sign, buffer + HASHSIZE + 4, SIGNSIZE);

    return 0;
}

int ota_verify_hash(int address, signature_t* signature) {
    UDPLGP("Verify hash\n");
    
    byte hash[HASHSIZE];
    ota_hash(address, signature->size, hash, file_first_byte[0]);
    
    if (memcmp(hash, signature->hash, HASHSIZE))
        ota_hash(address, signature->size, hash, 0xff);
    
    return memcmp(hash, signature->hash, HASHSIZE);
}

void ota_write_status(char * version) {
    UDPLGP("Set version: %s\n", version);
    
    sysparam_set_string("ota_version", version);
}

int ota_boot(void) {
    byte bootrom;
    rboot_get_last_boot_rom(&bootrom);
    UDPLGP("Current ROM is %d\n", bootrom);
    return 1 - bootrom;
}

void ota_reboot() {
    UDPLGP("Restarting...\n");

    vTaskDelay(200 / portTICK_PERIOD_MS);
    sdk_system_restart();
}

void ota_temp_boot() {
    UDPLGP("Select ROM 1\n");
    
    rboot_set_temp_rom(1);
    ota_reboot();
}
