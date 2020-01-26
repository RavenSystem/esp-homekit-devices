/*  (c) 2018-2019 HomeAccessoryKid */
#include <stdlib.h>  //for UDPLGP
#include <stdio.h>
#include <string.h>

#include <lwip/sockets.h>
#include <lwip/api.h>
#include <esp8266.h>
#include <wolfssl/ssl.h>
#include <wolfssl/wolfcrypt/types.h>	    // needed by wolfSSL_check_domain_name()
#include <wolfssl/wolfcrypt/logging.h>
#include <wolfssl/wolfcrypt/sha512.h>
#include <wolfssl/wolfcrypt/ecc.h>
#include <ota.h>

#include <sntp.h>
//#include <time.h> //included in sntp.h
#include <spiflash.h>
#include <sysparam.h>
#include <rboot-api.h>

#include <udplogger.h>

static int verify = 1;
static byte file_first_byte[]={0xff};
ecc_key prvecckey;
ecc_key pubecckey;

WOLFSSL_CTX* ctx;

#ifdef DEBUG_WOLFSSL    
void MyLoggingCallback(const int logLevel, const char* const logMessage) {
    /*custom logging function*/
    UDPLGP("loglevel: %d - %s\n",logLevel, logMessage);
}
#endif

bool userbeta=0;
bool otabeta=0;


void  ota_new_layout() {
    sysparam_status_t status;

    status = sysparam_init(SYSPARAMSECTOR, 0);
    if (status == SYSPARAM_NOTFOUND) {
        status = sysparam_create_area(SYSPARAMSECTOR, 2, false);
        if (status == SYSPARAM_OK) {
            status = sysparam_init(SYSPARAMSECTOR, 0);
        }
    }
    if (status != SYSPARAM_OK) {
        printf("WARNING: Could not initialize sysparams (%d)!\n", status);
    }
}

void  ota_init() {
    UDPLGP("--- ota_init\n");

    ip_addr_t target_ip;
    int ret;
    
    //using beta = pre-releases?
    #ifdef OTABETA
    sysparam_set_bool("lcm_beta", 1);
    #endif
    sysparam_get_bool("lcm_beta", &otabeta);
    sysparam_get_bool("ota_beta", &userbeta);
    
    UDPLGP("userbeta=\'%d\' otabeta=\'%d\'\n",userbeta,otabeta);
    
    //rboot setup
    rboot_config conf;
    conf=rboot_get_config();
    if (conf.count!=2 || conf.roms[0]!=BOOT0SECTOR || conf.roms[1]!=BOOT1SECTOR || conf.current_rom!=0) {
        conf.count =2;   conf.roms[0] =BOOT0SECTOR;   conf.roms[1] =BOOT1SECTOR;   conf.current_rom =0;
        rboot_set_config(&conf);
    }
    
    //time support
    const char *servers[] = {SNTP_SERVERS};
	sntp_set_update_delay(24*60*60000); //SNTP will request an update every 24 hour
	//const struct timezone tz = {1*60, 0}; //Set GMT+1 zone, daylight savings off
	//sntp_initialize(&tz);
	sntp_initialize(NULL);
	sntp_set_servers(servers, sizeof(servers) / sizeof(char*)); //Servers must be configured right after initialization

#ifdef DEBUG_WOLFSSL    
    if (wolfSSL_SetLoggingCb(MyLoggingCallback)) UDPLGP("error setting debug callback\n");
#endif
    
    wolfSSL_Init();

    ctx = wolfSSL_CTX_new(wolfTLSv1_2_client_method());
    if (!ctx) {
        //error
    }
    extern int active_cert_sector;
    extern int backup_cert_sector;
    // set active_cert_sector
    // first byte of the sector is its state:
    // 0xff backup being evaluated
    // 0x30 active sector
    // 0x00 deactivated
    byte fourbyte[4];
    active_cert_sector=HIGHERCERTSECTOR;
    backup_cert_sector=LOWERCERTSECTOR;
    if (!spiflash_read(active_cert_sector, (byte *)fourbyte, 4)) { //get first 4 active
        UDPLGP("error reading flash\n");
    } // if OTHER  vvvvvv sector active
    if (fourbyte[0]!=0x30 || fourbyte[1]!=0x76 || fourbyte[2]!=0x30 || fourbyte[3]!=0x10 ) {
        active_cert_sector=LOWERCERTSECTOR;
        backup_cert_sector=HIGHERCERTSECTOR;
        if (!spiflash_read(active_cert_sector, (byte *)fourbyte, 4)) {
            UDPLGP("error reading flash\n");
        }
        if (fourbyte[0]!=0x30 || fourbyte[1]!=0x76 || fourbyte[2]!=0x30 || fourbyte[3]!=0x10 ) {
            active_cert_sector=0;
            backup_cert_sector=0;
        }
    }
    UDPLGP("active_sector: 0x%x\n",active_cert_sector);
    ota_set_verify(0);
    UDPLGP("--- DNS: ");
    ret = netconn_gethostbyname(HOST, &target_ip);
    while(ret) {
        UDPLGP("%d",ret);
        vTaskDelay(200);
        ret = netconn_gethostbyname(HOST, &target_ip);
    }
    UDPLGP("done!\n");
}

int ota_get_privkey() {
    UDPLGP("--- ota_get_privkey\n");
    
    byte buffer[PKEYSIZE]; //maybe 49 bytes would be enough
    int ret;
    unsigned int idx;
    int length;
    
    //load private key as produced by openssl
    if (!spiflash_read(backup_cert_sector, (byte *)buffer, 24)) {
        UDPLGP("error reading flash\n");    return -1;
    }
    if (buffer[0]!=0x30 || buffer[1]!=0x81) return -2; //not a valid keyformat
    if (buffer[3]!=0x02 || buffer[4]!=0x01 || buffer[5]!=0x01) return -2; //not a valid keyformat
    if (buffer[6]!=0x04) return -2; //not a valid keyformat
    idx=7;
    length=buffer[idx++]; //bitstring start
    
    if (!spiflash_read(backup_cert_sector+idx, (byte *)buffer, length)) {
        UDPLGP("error reading flash\n");    return -1;
    }
    for (idx=0;idx<length;idx++) printf(" %02x",buffer[idx]);
    wc_ecc_init(&prvecckey);
    ret=wc_ecc_import_private_key_ex(buffer, length, NULL, 0, &prvecckey,ECC_SECP384R1);
    printf("\nret: %d\n",ret);
    
    /*
    */
    return ret;
}

int ota_get_pubkey(int sector) { //get the ecdsa key from the indicated sector, report filesize
    UDPLGP("--- ota_get_pubkey\n");
    
    byte buf[PKEYSIZE];
    byte * buffer=buf;
    int length,ret;
    //load public key as produced by openssl
    if (!spiflash_read(sector, (byte *)buffer, PKEYSIZE)) {
        UDPLGP("error reading flash\n");    return -1;
    }
    //do not test the first byte since else the key-update routine will not be able to collect a key
    if (buffer[ 1]!=0x76 || buffer[ 2]!=0x30 || buffer[ 3]!=0x10) return -2; //not a valid keyformat
    if (buffer[20]!=0x03 || buffer[21]!=0x62 || buffer[22]!=0x00) return -2; //not a valid keyformat
    length=97;
    
    int idx; for (idx=0;idx<length;idx++) printf(" %02x",buffer[idx+23]);
    wc_ecc_init(&pubecckey);
    ret=wc_ecc_import_x963_ex(buffer+23,length,&pubecckey,ECC_SECP384R1);
    printf("\n");
    UDPLGP("ret: %d\n",ret);

    if (!ret)return PKEYSIZE; else return ret;
}

int ota_verify_pubkey(void) { //check if public and private key are a pair
    UDPLGP("--- ota_verify_pubkey\n");
    
    byte hash[HASHSIZE];
    WC_RNG rng;
    wc_RNG_GenerateBlock(&rng, hash, HASHSIZE);
    //int i; printf("hash: "); for (i=0;i<HASHSIZE;i++) printf("%02x ",hash[i]); printf("\n");
    
    int answer;
    unsigned int siglen=SIGNSIZE;
    byte signature[SIGNSIZE];

    wc_ecc_sign_hash(hash, HASHSIZE, signature, &siglen, &rng, &prvecckey);
    wc_ecc_verify_hash(signature, siglen, hash, HASHSIZE, &answer, &pubecckey);
    
    UDPLGP("key valid: %d\n",answer);
        
    return answer-1;
}

void ota_hash(int start_sector, int filesize, byte * hash, byte first_byte) {
    UDPLGP("--- ota_hash\n");
    
    int bytes;
    byte buffer[1024];
    Sha384 sha;
    
    wc_InitSha384(&sha);
    //printf("bytes: ");
    for (bytes=0;bytes<filesize-1024;bytes+=1024) {
        //printf("%d ",bytes);
        if (!spiflash_read(start_sector+bytes, (byte *)buffer, 1024)) {
            UDPLGP("error reading flash\n");   break;
        }
        if (!bytes && first_byte!=0xff) buffer[0]=first_byte;
        wc_Sha384Update(&sha, buffer, 1024);
    }
    //printf("%d\n",bytes);
    if (!spiflash_read(start_sector+bytes, (byte *)buffer, filesize-bytes)) {
        UDPLGP("error reading flash\n");
    }
    if (!bytes && first_byte!=0xff) buffer[0]=first_byte;
    //printf("filesize %d\n",filesize);
    wc_Sha384Update(&sha, buffer, filesize-bytes);
    wc_Sha384Final(&sha, hash);
}

void ota_sign(int start_sector, int filesize, signature_t* signature, char* file) {
    UDPLGP("--- ota_sign\n");
    
    unsigned int i,siglen=SIGNSIZE;
    WC_RNG rng;

    ota_hash(start_sector, filesize, signature->hash, 0xff); // 0xff=no special first byte action
    wc_ecc_sign_hash(signature->hash, HASHSIZE, signature->sign, &siglen, &rng, &prvecckey);
    printf("echo "); for (i=0;i<HASHSIZE;i++) printf("%02x ",signature->hash[i]); printf("> x.hex\n");
    printf("echo %08x >>x.hex\n",filesize);
    printf("echo "); for (i=0;i<siglen  ;i++) printf("%02x ",signature->sign[i]); printf(">>x.hex\n");
    printf("xxd -r -p x.hex > %s.sig\n",file);  printf("rm x.hex\n");
}

int ota_compare(char* newv, char* oldv) { //(if equal,0) (if newer,1) (if pre-release or older,-1)
    UDPLGP("--- ota_compare ");
    printf("\n");
    char* dot;
    int valuen=0,valueo=0;
    char news[MAXVERSIONLEN],olds[MAXVERSIONLEN];
    char * new=news;
    char * old=olds;
    int result=0;
    
    if (strcmp(newv,oldv)) { //https://semver.org/#spec-item-11
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
    UDPLGP("%s with %s=%d\n",newv,oldv,result);
    return result;
}

int local_port=0;
static int ota_connect(char* host, int port, int *socket, WOLFSSL** ssl) {
    UDPLGP("--- ota_connect LocalPort=");
    int ret;
    ip_addr_t target_ip;
    struct sockaddr_in sock_addr;
    unsigned char initial_port[2];
    WC_RNG rng;
    
    if (!local_port) {
        wc_RNG_GenerateBlock(&rng, initial_port, 2);
        local_port=(256*initial_port[0]+initial_port[1])|0xc000;
    }
    UDPLGP("%04x DNS",local_port);
    ret = netconn_gethostbyname(host, &target_ip);
    while(ret) {
        printf("%d",ret);
        vTaskDelay(200);
        ret = netconn_gethostbyname(host, &target_ip);
    }
    UDPLGP(" IP:%d.%d.%d.%d ", (unsigned char)((target_ip.addr & 0x000000ff) >> 0),
                              (unsigned char)((target_ip.addr & 0x0000ff00) >> 8),
                              (unsigned char)((target_ip.addr & 0x00ff0000) >> 16),
                              (unsigned char)((target_ip.addr & 0xff000000) >> 24));
    //printf("create socket ......");
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
    if (local_port==0x10000) local_port=0xc000;
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

    if (verify) ret=wolfSSL_check_domain_name(*ssl, host);
//wolfSSL_Debugging_OFF();

    UDPLGP("to %s port %d..", host, port);
    ret = wolfSSL_connect(*ssl);
    if (ret != SSL_SUCCESS) {
        UDPLGP("failed, return [-0x%x]\n", -ret);
        ret=wolfSSL_get_error(*ssl,ret);
        UDPLGP("wolfSSL_send error = %d\n", ret);
        return -1;
    }
    UDPLGP("OK\n");
    return 0;

}

int   ota_load_user_app(char * *repo, char * *version, char * *file) {
    UDPLGP("--- ota_load_user_app\n");
    sysparam_status_t status;
    char *value;

    status = sysparam_get_string("ota_repo", &value);
    if (status == SYSPARAM_OK) {
        *repo=value;
    } else return -1;
    status = sysparam_get_string("ota_version", &value);
    if (status == SYSPARAM_OK) {
        *version=value;
    } else return -1;
    status = sysparam_get_string("ota_file", &value);
    if (status == SYSPARAM_OK) {
        *file=value;
    } else return -1;

    UDPLGP("user_repo=\'%s\' user_version=\'%s\' user_file=\'%s\'\n",*repo,*version,*file);
    return 0;
}

void  ota_set_verify(int onoff) {
    UDPLGP("--- ota_set_verify...");
    int ret=0;
    byte abyte[1];
    
    if (onoff) {
        UDPLGP("ON\n");
        if (verify==0) {
            verify= 1;
            do {
                if (!spiflash_read(active_cert_sector+PKEYSIZE+(ret++), (byte *)abyte, 1)) {
                    UDPLGP("error reading flash\n");
                    break;
                }
            } while (abyte[0]!=0xff); ret--;
            UDPLGP("certs size: %d\n",ret);
            byte *certs=malloc(ret);
            spiflash_read(active_cert_sector+PKEYSIZE, (byte *)certs, ret);

            ret=wolfSSL_CTX_load_verify_buffer(ctx, certs, ret, SSL_FILETYPE_PEM);
            if ( ret != SSL_SUCCESS) {
                UDPLGP("fail cert loading, return %d\n", ret);
            }
            free(certs);
            
            time_t ts;
            do {
                ts = time(NULL);
                if (ts == ((time_t)-1)) printf("ts=-1, ");
                vTaskDelay(1);
            } while (!(ts>1073741823)); //2^30-1 which is supposed to be like 2004
            UDPLGP("TIME: %s", ctime(&ts)); //we need to have the clock right to check certificates
            
            wolfSSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
        }
    } else {
        UDPLGP("OFF\n");
        if (verify==1) {
            verify= 0;
            wolfSSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
        }
    }
}

int   ota_get_file_ex(char * repo, char * version, char * file, int sector, byte * buffer, int bufsz); //prototype needed
char* ota_get_version(char * repo) {
    UDPLGP("--- ota_get_version\n");

    char* version=NULL;
    char prerelease[64]; 
    int retc, ret=0;
    int httpcode;
    WOLFSSL*     ssl;
    int socket;
    //host=begin(repo);
    //mid =end(repo)+blabla+version
    char* location;
    char recv_buf[RECV_BUF_LEN];
    int  send_bytes; //= sizeof(send_data);
    
    strcat(strcat(strcat(strcat(strcat(strcpy(recv_buf, \
        REQUESTHEAD),repo),"/releases/latest"),REQUESTTAIL),HOST),CRLFCRLF);
    send_bytes=strlen(recv_buf);
    //printf("%s\n",recv_buf);

    retc = ota_connect(HOST, HTTPS_PORT, &socket, &ssl);  //release socket and ssl when ready
    
    if (!retc) {
        UDPLGP("%s",recv_buf);
        ret = wolfSSL_write(ssl, recv_buf, send_bytes);
        if (ret > 0) {
            printf("sent OK\n");

            //wolfSSL_shutdown(ssl); //by shutting down the connection before even reading, we reduce the payload to the minimum
            ret = wolfSSL_peek(ssl, recv_buf, RECV_BUF_LEN - 1);
            if (ret > 0) {
                recv_buf[ret]=0; //error checking
                //printf("%s\n",recv_buf);

                location=strstr(recv_buf,"HTTP/1.1 ");
                strchr(location,' ')[0]=0;
                location+=9; //flush "HTTP/1.1 "
                httpcode=atoi(location);
                UDPLGP("HTTP returns %d for ",httpcode);
                if (httpcode!=302) {
                    wolfSSL_free(ssl);
                    lwip_close(socket);
                    return "404";
                }
                recv_buf[strlen(recv_buf)]=' '; //for further headers

                location=strstr(recv_buf,"Location: ");
                strchr(location,'\r')[0]=0;
                //printf("%s\n",location);
                location=strstr(location,"tag/");
                version=malloc(strlen(location+4));
                strcpy(version,location+4);
                printf("%s@version:\"%s\" according to latest release\n",repo,version);
            } else {
                UDPLGP("failed, return [-0x%x]\n", -ret);
                ret = wolfSSL_get_error(ssl,ret);
                UDPLGP("wolfSSL_send error = %d\n", ret);
            }
        } else {
            UDPLGP("failed, return [-0x%x]\n", -ret);
            ret = wolfSSL_get_error(ssl,ret);
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

//     if (retc) return retc;
//     if (ret <= 0) return ret;

    //find latest-pre-release if joined beta program
    if ((userbeta && strcmp(OTAREPO, repo)) || (otabeta && !strcmp(OTAREPO, repo))) {
        prerelease[63] = 0;
        ret = ota_get_file_ex(repo,version, "latest-pre-release", 0, (byte *) prerelease, 63);
        if (ret > 0) {
            prerelease[ret] = 0;
            free(version);
            version = malloc(strlen(prerelease) + 1);
            strcpy(version, prerelease);
        }
    }
    
    if (ota_boot() && ota_compare(version,OTAVERSION) < 0) { //this acts when setting up a new version
        free(version);
        version = malloc(strlen(OTAVERSION) + 1);
        strcpy(version, OTAVERSION);
    }
    
    UDPLGP("%s@version:\"%s\"\n", repo, version);
    return version;
}

int   ota_get_file_ex(char * repo, char * version, char * file, int sector, byte * buffer, int bufsz) { //number of bytes
    UDPLGP("--- ota_get_file_ex\n");
    
    int retc, ret=0, slash;
    WOLFSSL*     ssl;
    int socket;
    //host=begin(repo);
    //mid =end(repo)+blabla+version
    char* location;
    char recv_buf[RECV_BUF_LEN];
    int  recv_bytes = 0;
    int  send_bytes; //= sizeof(send_data);
    int  length=1;
    int  clength;
    int  collected=0;
    int  writespace=0;
    int  header;
    
    if (sector==0 && buffer==NULL) return -5; //needs to be either a sector or a signature
    
    strcat(strcat(strcat(strcat(strcat(strcat(strcat(strcat(strcpy(recv_buf, \
        REQUESTHEAD),repo),"/releases/download/"),version),"/"),file),REQUESTTAIL),HOST),CRLFCRLF);
    send_bytes=strlen(recv_buf);
    UDPLGP("%s",recv_buf);

    retc = ota_connect(HOST, HTTPS_PORT, &socket, &ssl);  //release socket and ssl when ready
    
    if (!retc) {
        ret = wolfSSL_write(ssl, recv_buf, send_bytes);
        if (ret > 0) {
            UDPLGP("sent OK\n");

            //wolfSSL_shutdown(ssl); //by shutting down the connection before even reading, we reduce the payload to the minimum
            ret = wolfSSL_peek(ssl, recv_buf, RECV_BUF_LEN - 1);
            if (ret > 0) {
                recv_buf[ret]=0; //error checking, e.g. not result=206
                printf("%s\n",recv_buf);
                location=strstr(recv_buf,"HTTP/1.1 ");
                strchr(location,' ')[0]=0;
                location+=9; //flush "HTTP/1.1 "
                slash=atoi(location);
                UDPLGP("HTTP returns %d\n",slash);
                if (slash!=302) {
                    wolfSSL_free(ssl);
                    lwip_close(socket);
                    return -1;
                }
                recv_buf[strlen(recv_buf)]=' '; //for further headers
                location=strstr(recv_buf,"Location: ");
                strchr(location,'\r')[0]=0;
                location+=18; //flush Location: https://
                //printf("%s\n",location);
            } else {
                UDPLGP("failed, return [-0x%x]\n", -ret);
                ret=wolfSSL_get_error(ssl,ret);
                UDPLGP("wolfSSL_send error = %d\n", ret);
            }
        } else {
            UDPLGP("failed, return [-0x%x]\n", -ret);
            ret=wolfSSL_get_error(ssl,ret);
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

    if (retc) return retc;
    if (ret <= 0) return ret;
    
    //process the Location
    strcat(location, REQUESTTAIL);
    slash=strchr(location,'/')-location;
    location[slash]=0; //cut behind the hostname
    char * host2=malloc(strlen(location));
    strcpy(host2,location);
    //printf("next host: %s\n",host2);

    retc = ota_connect(host2, HTTPS_PORT, &socket, &ssl);  //release socket and ssl when ready

    strcat(strcat(location+slash+1,host2),RANGE); //append hostname and range to URI    
    location+=slash-4;
    memcpy(location,REQUESTHEAD,5);
    char * getlinestart=malloc(strlen(location));
    strcpy(getlinestart,location);
    //printf("request:\n%s\n",getlinestart);
    //if (!retc) {
    while (collected<length) {
        sprintf(recv_buf,"%s%d-%d%s",getlinestart,collected,collected+4095,CRLFCRLF);
        send_bytes=strlen(recv_buf);
        //printf("request:\n%s\n",recv_buf);
        printf("send request......");
        ret = wolfSSL_write(ssl, recv_buf, send_bytes);
        recv_bytes=0;
        if (ret > 0) {
            printf("OK\n");

            header=1;
            memset(recv_buf,0,RECV_BUF_LEN);
            //wolfSSL_Debugging_ON();
            do {
                ret = wolfSSL_read(ssl, recv_buf, RECV_BUF_LEN - 1);
                if (ret > 0) {
                    if (header) {
                        //printf("%s\n-------- %d\n", recv_buf, ret);
                        //parse Content-Length: xxxx
                        location=strstr(recv_buf,"Content-Length: ");
                        strchr(location,'\r')[0]=0;
                        location+=16; //flush Content-Length: //
                        clength=atoi(location);
                        location[strlen(location)]='\r'; //in case the order changes
                        //parse Content-Range: bytes xxxx-yyyy/zzzz
                        location=strstr(recv_buf,"Content-Range: bytes ");
                        strchr(location,'\r')[0]=0;
                        location+=21; //flush Content-Range: bytes //
                        location=strstr(location,"/"); location++; //flush /
                        length=atoi(location);
                        //verify if last bytes are crlfcrlf else header=1
                    } else {
                        recv_bytes += ret;
                        if (sector) { //write to flash
                            if (writespace<ret) {
                                UDPLGP("erasing@0x%05x>", sector+collected);
                                if (!spiflash_erase_sector(sector+collected)) return -6; //erase error
                                writespace+=SECTORSIZE;
                            }
                            if (collected) {
                                if (!spiflash_write(sector+collected, (byte *)recv_buf,   ret  )) return -7; //write error
                            } else { //at the very beginning, do not write the first byte yet but store it for later
                                file_first_byte[0]=(byte)recv_buf[0];
                                if (!spiflash_write(sector+1        , (byte *)recv_buf+1, ret-1)) return -7; //write error
                            }
                            writespace-=ret;
                        } else { //buffer
                            if (ret>bufsz) return -8; //too big
                            memcpy(buffer,recv_buf,ret);
                        }
                        collected+=ret;
                        int i;
                        for (i=0;i<3;i++) printf("%02x", recv_buf[i]);
                        printf("...");
                        for (i=3;i>0;i--) printf("%02x", recv_buf[ret-i]);
                        printf(" ");
                    }
                } else {
                    if (ret) {ret=wolfSSL_get_error(ssl,ret); UDPLGP("error %d\n",ret);}
                    if (!ret && collected<length) retc = ota_connect(host2, HTTPS_PORT, &socket, &ssl); //memory leak?
                    break;
                }
                header=0; //move to header section itself
            } while(recv_bytes<clength);
            printf(" so far collected %d bytes\n", collected);
            UDPLOG(" collected %d bytes\r",        collected);
        } else {
            printf("failed, return [-0x%x]\n", -ret);
            ret=wolfSSL_get_error(ssl,ret);
            printf("wolfSSL_send error = %d\n", ret);
            if (ret==-308) {
                retc = ota_connect(host2, HTTPS_PORT, &socket, &ssl); //dangerous for eternal connecting? memory leak?
            } else {
                break; //give up?
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

void  ota_finalize_file(int sector) {
    UDPLGP("--- ota_finalize_file\n");

    if (!spiflash_write(sector, file_first_byte, 1)) UDPLGP("error writing flash\n");
    //TODO: add verification and retry and if wrong return status...
}

int   ota_get_file(char * repo, char * version, char * file, int sector) { //number of bytes
    UDPLGP("--- ota_get_file\n");
    return ota_get_file_ex(repo,version,file,sector,NULL,0);
}

int   ota_get_hash(char * repo, char * version, char * file, signature_t* signature) {
    UDPLGP("--- ota_get_hash\n");
    int ret;
    byte buffer[HASHSIZE+4+SIGNSIZE];
    char * signame=malloc(strlen(file)+5);
    strcpy(signame,file);
    strcat(signame,".sig");
    memset(signature->hash,0,HASHSIZE);
    memset(signature->sign,0,SIGNSIZE);
    ret=ota_get_file_ex(repo,version,signame,0,buffer,HASHSIZE+4+SIGNSIZE);
    free(signame);
    if (ret<0) return ret;
    memcpy(signature->hash,buffer,HASHSIZE);
    signature->size=((buffer[HASHSIZE]*256 + buffer[HASHSIZE+1])*256 + buffer[HASHSIZE+2])*256 + buffer[HASHSIZE+3];
    if (ret>HASHSIZE+4) memcpy(signature->sign,buffer+HASHSIZE+4,SIGNSIZE);

    return 0;
}

int   ota_verify_hash(int address, signature_t* signature) {
    UDPLGP("--- ota_verify_hash\n");
    
    byte hash[HASHSIZE];
    ota_hash(address, signature->size, hash, file_first_byte[0]);
//     int i;
//     printf("signhash:"); for (i=0;i<HASHSIZE;i++) printf(" %02x",signature->hash[i]); printf("\n");
//     printf("calchash:"); for (i=0;i<HASHSIZE;i++) printf(" %02x",           hash[i]); printf("\n");
    
    if (memcmp(hash,signature->hash,HASHSIZE)) ota_hash(address, signature->size, hash, 0xff);
    
    return memcmp(hash,signature->hash,HASHSIZE);
}

int   ota_verify_signature(signature_t* signature) {
    UDPLGP("--- ota_verify_signature\n");
    
    int answer=0;

    wc_ecc_verify_hash(signature->sign, SIGNSIZE, signature->hash, HASHSIZE, &answer, &pubecckey);
    UDPLGP("signature valid: %d\n",answer);
        
    return answer-1;
}

void  ota_kill_file(int sector) {
    UDPLGP("--- ota_kill_file\n");

    byte zero[]={0x00};
    if (!spiflash_write(sector, zero, 1)) UDPLGP("error writing flash\n");
}

void  ota_swap_cert_sector() {
    UDPLGP("--- ota_swap_cert_sector\n");
    
    ota_kill_file(active_cert_sector);
    ota_finalize_file(backup_cert_sector);
    if (active_cert_sector==HIGHERCERTSECTOR) {
        active_cert_sector=LOWERCERTSECTOR;
        backup_cert_sector=HIGHERCERTSECTOR;
    } else {
        active_cert_sector=HIGHERCERTSECTOR;
        backup_cert_sector=LOWERCERTSECTOR;
    }
}

void  ota_write_status(char * version) {
    UDPLGP("--- ota_write_status\n");
    
    sysparam_set_string("ota_version", version);
}

int   ota_boot(void) {
    UDPLGP("--- ota_boot...");
    byte bootrom;
    rboot_get_last_boot_rom(&bootrom);
    UDPLGP("%d\n",bootrom);
    return 1-bootrom;
}

void  ota_temp_boot(void) {
    UDPLGP("--- ota_temp_boot\n");
    
    rboot_set_temp_rom(1);
    vTaskDelay(20); //allows UDPLOG to flush
    sdk_system_restart();
}

void  ota_reboot(void) {
    UDPLGP("--- ota_reboot\n");

    vTaskDelay(20); //allows UDPLOG to flush
    sdk_system_restart();
}
