/*
 * Home Accessory Architect OTA Installer
 *
 * Copyright 2020-2022 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

/*
 * Based on Life-Cycle-Manager (LCM) by HomeAccessoryKid (@HomeACcessoryKid), licensed under Apache License 2.0.
 * https://github.com/HomeACcessoryKid/life-cycle-manager
 */

#ifndef __HAA_OTA_H__
#define __HAA_OTA_H__

#include "header.h"

#define OTAREPO                 "github.com/RavenSystem/haa/releases/latest/download"

#ifdef ESP_PLATFORM
#define HAA_FINAL_NAME          "_"HAA_CHIP_NAME""HAA_SINGLE_CORE_SUFIX""HAA_XTAL26_SUFIX
#else
#define HAA_FINAL_NAME          ""
#endif

#define OTAMAINFILE             "otamain"HAA_FINAL_NAME".bin"
#define OTABOOTFILE             "haaboot"HAA_FINAL_NAME".bin"
#define HAAMAINFILE             "haamain"HAA_FINAL_NAME".bin"

#define OTAVERSIONFILE          "otaversion"
#define HAAVERSIONFILE          "haaversion"

#define SIGNFILESUFIX           ".sec"
#define VERSIONSTRINGLEN        (15)

#define MAX_GLOBAL_TRIES        (2)
#define TRIES_PARTIAL_COUNT_MAX (5)
#define MAX_DOWNLOAD_FILE_TRIES (4)

#define HTTPS_PORT              (443)
#define REQUESTHEAD             "GET /"
#define REQUESTTAIL             " HTTP/1.1\r\nHost: "
#define CRLFCRLF                "\r\n\r\n"
#define RECV_BUF_LEN            (1390)
#define HEADER_BUFFER_LEN       (8000)
#define HOST_LEN                (128)
#define LOCATION_LEN            (1198)
#define RANGE                   "\r\nRange: bytes="

#define MAX_302_JUMPS           (10)

#define HASHSIZE                (48)        // SHA-384
#define SIGNSIZE                (104)       // ECDSA r+s in ASN1 format secP384r1

void ota_init(char* repo, const bool is_ssl);
char* ota_get_version(char* repo, char* version_file, uint16_t port, const bool is_ssl);
int ota_get_file_part(char* repo, char* file, int sector, uint16_t port, const bool is_ssl, int *collected);   // Return number of bytes
int ota_get_file(char* repo, char* file, int sector, uint16_t port, const bool is_ssl);   // Return number of bytes
void ota_finalize_file(int sector);
int ota_get_sign(char* repo, char* file, uint8_t* signature, uint16_t port, const bool is_ssl);
int ota_verify_sign(int address, int file_size, uint8_t* signature);
void ota_reboot(const uint8_t partition);

#endif // __HAA_OTA_H__
