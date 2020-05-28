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

#ifndef __HAA_OTA_H__
#define __HAA_OTA_H__

#define OTAREPO                 "github.com/RavenSystem/haa/releases/latest/download"
#define OTAMAINFILE             "otamain.bin"
#define OTABOOTFILE             "haaboot.bin"
#define HAAMAINFILE             "haamain.bin"
#define OTAVERSIONFILE          "otaversion"
#define HAAVERSIONFILE          "haaversion"
#define SIGNFILESUFIX           ".sec"
#define VERSIONFILESIZE         9

#define MAX_TRIES               2

#define SYSPARAMSECTOR          0xF3000
#define SYSPARAMSIZE            8

#define SECTORSIZE              4096
#define BOOT0SECTOR             0x02000
#define BOOT1SECTOR             0x8D000     // Must match the program1.ld value

#define HTTPS_PORT              443
#define FAILED                  "failed\n"
#define REQUESTHEAD             "GET /"
#define REQUESTTAIL             " HTTP/1.1\r\nHost: "
#define CRLFCRLF                "\r\n\r\n"
#define RECV_BUF_LEN            1025  // current length of amazon URL 724
#define HOST_LEN                256
#define RANGE                   "\r\nRange: bytes="

#define SNTP_SERVERS            "0.pool.ntp.org", "1.pool.ntp.org", "2.pool.ntp.org", "3.pool.ntp.org"
#define MAX_302_JUMPS           6

#define HASHSIZE                48      //SHA-384
#define SIGNSIZE                104     //ECDSA r+s in ASN1 format secP384r1

typedef unsigned char byte;

void ota_init(char* repo, const bool is_ssl);
char* ota_get_version(char* repo, char* version_file, uint16_t port, const bool is_ssl);
int ota_get_file(char* repo, char* file, int sector, uint16_t port, const bool is_ssl);   // Return number of bytes
void ota_finalize_file(int sector);
int ota_get_sign(char* repo, char* file, byte* signature, uint16_t port, const bool is_ssl);
int ota_verify_sign(int address, int file_size, byte* signature);
void ota_temp_boot();
void ota_reboot();

#endif // __HAA_OTA_H__
