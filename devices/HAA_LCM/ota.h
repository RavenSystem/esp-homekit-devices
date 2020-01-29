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
#ifndef __OTA_H__
#define __OTA_H__

#ifndef OTAVERSION
    #error You must set OTAVERSION=x.y.z of the ota code to match github version tag x.y.z
#endif

#define OTAREPO                 "RavenSystem/haa_ota"
#define MAINFILE                "main.bin"
#define BOOTFILE                "haaboot.bin"

#define USERREPO                "RavenSystem/haa"
#define USERFILE                "main.bin"

#define SYSPARAMSECTOR          0xF3000
#define SYSPARAMSIZE            8

#define SECTORSIZE              4096
#define BOOT0SECTOR             0x02000
#define BOOT1SECTOR             0x8D000 //must match the program1.ld value!!

#define HOST                    "github.com"
#define HTTPS_PORT              443
#define FAILED                  "failed\n"
#define REQUESTHEAD             "GET /"
#define REQUESTTAIL             " HTTP/1.1\r\nHost: "
#define CRLFCRLF                "\r\n\r\n"
#define RECV_BUF_LEN            1025  // current length of amazon URL 724
#define RANGE                   "\r\nRange: bytes="
#define MAXVERSIONLEN           16
#define SNTP_SERVERS            "0.pool.ntp.org", "1.pool.ntp.org", "2.pool.ntp.org", "3.pool.ntp.org"

#define ECDSAKEYLENGTHMAX       128 //to be verified better, example is 120 bytes secP384r1
#define HASHSIZE                48  //SHA-384
#define SIGNSIZE                104  //ECDSA r+s in ASN1 format secP384r1
#define PKEYSIZE                120  //size of a pub key
#define KEYNAME                 "public-%d.key"
#define KEYNAMELEN              16 //allows for 9999 keys

typedef unsigned char byte;

typedef struct {
    byte hash[HASHSIZE];
    unsigned int   size; //32 bit
    byte sign[SIGNSIZE];
} signature_t;

void ota_init();
int ota_compare(char* newv, char* oldv);
int ota_load_user_app(char * *repo, char * *version, char * *file);
char* ota_get_version(char * repo);
int ota_get_file(char * repo, char * version, char * file, int sector); //number of bytes
void ota_finalize_file(int sector);
int ota_get_hash(char * repo, char * version, char * file, signature_t* signature);
int ota_verify_hash(int address, signature_t* signature);
void ota_write_status(char * version);
int ota_boot(void);
void ota_temp_boot(void);
void ota_reboot(void);

#endif // __OTA_H__
