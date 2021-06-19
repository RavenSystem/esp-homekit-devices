/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */


/*
This is a 'captive portal' DNS server: it basically replies with a fixed IP (in this case:
the one of the SoftAP interface of this ESP module) for any and all DNS queries. This can 
be used to send mobile phones, tablets etc which connect to the ESP in AP mode directly to
the internal webserver.
*/

#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <espressif/esp_wifi.h>

#include <libesphttpd/platform.h>
#ifdef FREERTOS

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "lwip/sockets.h"
#include "lwip/err.h"
static int sockFd;
#endif


#define DNS_LEN 512

typedef struct __attribute__ ((packed)) {
	uint16_t id;
	uint8_t flags;
	uint8_t rcode;
	uint16_t qdcount;
	uint16_t ancount;
	uint16_t nscount;
	uint16_t arcount;
} DnsHeader;


typedef struct __attribute__ ((packed)) {
	uint8_t len;
	uint8_t data;
} DnsLabel;


typedef struct __attribute__ ((packed)) {
	//before: label
	uint16_t type;
	uint16_t class;
} DnsQuestionFooter;


typedef struct __attribute__ ((packed)) {
	//before: label
	uint16_t type;
	uint16_t class;
	uint32_t ttl;
	uint16_t rdlength;
	//after: rdata
} DnsResourceFooter;

typedef struct __attribute__ ((packed)) {
	uint16_t prio;
	uint16_t weight;
} DnsUriHdr;


#define FLAG_QR (1<<7)
#define FLAG_AA (1<<2)
#define FLAG_TC (1<<1)
#define FLAG_RD (1<<0)

#define QTYPE_A  1
#define QTYPE_NS 2
#define QTYPE_CNAME 5
#define QTYPE_SOA 6
#define QTYPE_WKS 11
#define QTYPE_PTR 12
#define QTYPE_HINFO 13
#define QTYPE_MINFO 14
#define QTYPE_MX 15
#define QTYPE_TXT 16
#define QTYPE_URI 256

#define QCLASS_IN 1
#define QCLASS_ANY 255
#define QCLASS_URI 256


//Function to put unaligned 16-bit network values
static void setn16(void *pp, int16_t n) {
	char *p=pp;
	*p++=(n>>8);
	*p++=(n&0xff);
}

//Function to put unaligned 32-bit network values
static void setn32(void *pp, int32_t n) {
	char *p=pp;
	*p++=(n>>24)&0xff;
	*p++=(n>>16)&0xff;
	*p++=(n>>8)&0xff;
	*p++=(n&0xff);
}

static uint16_t my_ntohs(uint16_t *in) {
	char *p=(char*)in;
	return ((p[0]<<8)&0xff00)|(p[1]&0xff);
}


//Parses a label into a C-string containing a dotted 
//Returns pointer to start of next fields in packet
static char* labelToStr(char *packet, char *labelPtr, int packetSz, char *res, int resMaxLen) {
	int i, j, k;
	char *endPtr=NULL;
	i=0;
	do {
		if ((*labelPtr&0xC0)==0) {
			j=*labelPtr++; //skip past length
			//Add separator period if there already is data in res
			if (i<resMaxLen && i!=0) res[i++]='.';
			//Copy label to res
			for (k=0; k<j; k++) {
				if ((labelPtr-packet)>packetSz) return NULL;
				if (i<resMaxLen) res[i++]=*labelPtr++;
			}
		} else if ((*labelPtr&0xC0)==0xC0) {
			//Compressed label pointer
			endPtr=labelPtr+2;
			int offset=my_ntohs(((uint16_t *)labelPtr))&0x3FFF;
			//Check if offset points to somewhere outside of the packet
			if (offset>packetSz) return NULL;
			labelPtr=&packet[offset];
		}
		//check for out-of-bound-ness
		if ((labelPtr-packet)>packetSz) return NULL;
	} while (*labelPtr!=0);
	res[i]=0; //zero-terminate
	if (endPtr==NULL) endPtr=labelPtr+1;
	return endPtr;
}


//Converts a dotted hostname to the weird label form dns uses.
static char *strToLabel(char *str, char *label, int maxLen) {
	char *len=label; //ptr to len byte
	char *p=label+1; //ptr to next label byte to be written
	while (1) {
		if (*str=='.' || *str==0) {
			*len=((p-len)-1);	//write len of label bit
			len=p;				//pos of len for next part
			p++;				//data ptr is one past len
			if (*str==0) break;	//done
			str++;
		} else {
			*p++=*str++;	//copy byte
//			if ((p-label)>maxLen) return NULL;	//check out of bounds
		}
	}
	*len=0;
	return p; //ptr to first free byte in resp
}


//Receive a DNS packet and maybe send a response back
#ifndef FREERTOS
static void captdnsRecv(void* arg, char *pusrdata, unsigned short length) {
	struct espconn *conn=(struct espconn *)arg;
#else
static void captdnsRecv(struct sockaddr_in *premote_addr, char *pusrdata, unsigned short length) {
#endif
	char buff[DNS_LEN];
	char reply[DNS_LEN];
	int i;
	char *rend=&reply[length];
	char *p=pusrdata;
	DnsHeader *hdr=(DnsHeader*)p;
	DnsHeader *rhdr=(DnsHeader*)&reply[0];
	p+=sizeof(DnsHeader);
//	printf("DNS packet: id 0x%X flags 0x%X rcode 0x%X qcnt %d ancnt %d nscount %d arcount %d len %d\n", 
//		my_ntohs(&hdr->id), hdr->flags, hdr->rcode, my_ntohs(&hdr->qdcount), my_ntohs(&hdr->ancount), my_ntohs(&hdr->nscount), my_ntohs(&hdr->arcount), length);
	//Some sanity checks:
	if (length>DNS_LEN) return; 								//Packet is longer than DNS implementation allows
	if (length<sizeof(DnsHeader)) return; 						//Packet is too short
	if (hdr->ancount || hdr->nscount || hdr->arcount) return;	//this is a reply, don't know what to do with it
	if (hdr->flags&FLAG_TC) return;								//truncated, can't use this
	//Reply is basically the request plus the needed data
	memcpy(reply, pusrdata, length);
	rhdr->flags|=FLAG_QR;
	for (i=0; i<my_ntohs(&hdr->qdcount); i++) {
		//Grab the labels in the q string
		p=labelToStr(pusrdata, p, length, buff, sizeof(buff));
		if (p==NULL) return;
		DnsQuestionFooter *qf=(DnsQuestionFooter*)p;
		p+=sizeof(DnsQuestionFooter);
		printf("DNS: Q (type 0x%X class 0x%X) for %s\n", my_ntohs(&qf->type), my_ntohs(&qf->class), buff);
		if (my_ntohs(&qf->type)==QTYPE_A) {
			//They want to know the IPv4 address of something.
			//Build the response.
			rend=strToLabel(buff, rend, sizeof(reply)-(rend-reply)); //Add the label
			if (rend==NULL) return;
			DnsResourceFooter *rf=(DnsResourceFooter *)rend;
			rend+=sizeof(DnsResourceFooter);
			setn16(&rf->type, QTYPE_A);
			setn16(&rf->class, QCLASS_IN);
			setn32(&rf->ttl, 0);
			setn16(&rf->rdlength, 4); //IPv4 addr is 4 bytes;
			/*
			 * Captive portal addresses:
			 *
			 * Apple iOS: 	http://captive.apple.com/hotspot-detect.html
			 * Android:	http://clients3.google.com/generate_204
			 * Android 5+:	http://connectivitycheck.android.com/generate_204
			 * Windows:	http://www.msftncsi.com/ncsi.txt
			 * Windows 10:	http://www.msftconnecttest.com/redirect
			 *
			 * If so, return current IP, otherwise return 0.0.0.0
			 */
			if ((strcmp(buff, "captive.apple.com")==0) || (strcmp(buff, "clients3.google.com")==0) || (strcmp(buff, "connectivitycheck.android.com")==0) || (strcmp(buff, "www.msftncsi.com")==0) || (strcmp(buff, "www.msftconnecttest.com")==0)) {
				//Grab the current IP of the softap interface
				struct ip_info info;
				sdk_wifi_get_ip_info(SOFTAP_IF, &info);
				*rend++=ip4_addr1(&info.ip);
				*rend++=ip4_addr2(&info.ip);
				*rend++=ip4_addr3(&info.ip);
				*rend++=ip4_addr4(&info.ip);
			} else if(strcmp(buff, "dns.msftncsi.com")==0) {
				*rend++=131;
				*rend++=107;
				*rend++=255;
				*rend++=255;
			} else {
				*rend++=0;
				*rend++=0;
				*rend++=0;
				*rend++=0;
			}
			setn16(&rhdr->ancount, my_ntohs(&rhdr->ancount)+1);
//			printf("Added A rec to resp. Resp len is %d\n", (rend-reply));
		} else if (my_ntohs(&qf->type)==QTYPE_NS) {
			//Give ns server. Basically can be whatever we want because it'll get resolved to our IP later anyway.
			rend=strToLabel(buff, rend, sizeof(reply)-(rend-reply)); //Add the label
			DnsResourceFooter *rf=(DnsResourceFooter *)rend;
			rend+=sizeof(DnsResourceFooter);
			setn16(&rf->type, QTYPE_NS);
			setn16(&rf->class, QCLASS_IN);
			setn16(&rf->ttl, 0);
			setn16(&rf->rdlength, 4);
			*rend++=2;
			*rend++='n';
			*rend++='s';
			*rend++=0;
			setn16(&rhdr->ancount, my_ntohs(&rhdr->ancount)+1);
//			printf("Added NS rec to resp. Resp len is %d\n", (rend-reply));
		} else if (my_ntohs(&qf->type)==QTYPE_URI) {
			//Give uri to us
			rend=strToLabel(buff, rend, sizeof(reply)-(rend-reply)); //Add the label
			DnsResourceFooter *rf=(DnsResourceFooter *)rend;
			rend+=sizeof(DnsResourceFooter);
			DnsUriHdr *uh=(DnsUriHdr *)rend;
			rend+=sizeof(DnsUriHdr);
			setn16(&rf->type, QTYPE_URI);
			setn16(&rf->class, QCLASS_URI);
			setn16(&rf->ttl, 0);
			setn16(&rf->rdlength, 4+16);
			setn16(&uh->prio, 10);
			setn16(&uh->weight, 1);
			memcpy(rend, "http://esp.nonet", 16);
			rend+=16;
			setn16(&rhdr->ancount, my_ntohs(&rhdr->ancount)+1);
//			printf("Added NS rec to resp. Resp len is %d\n", (rend-reply));
		}
	}
	//Send the response
#ifndef FREERTOS
	remot_info *remInfo=NULL;
	//Send data to port/ip it came from, not to the ip/port we listen on.
	if (espconn_get_connection_info(conn, &remInfo, 0)==ESPCONN_OK) {
		conn->proto.udp->remote_port=remInfo->remote_port;
		memcpy(conn->proto.udp->remote_ip, remInfo->remote_ip, sizeof(remInfo->remote_ip));
	}
	espconn_sendto(conn, (uint8*)reply, rend-reply);
#else
	sendto(sockFd,(uint8*)reply, rend-reply, 0, (struct sockaddr *)premote_addr, sizeof(struct sockaddr_in));
#endif
}

#ifdef FREERTOS
static void captdnsTask(void *pvParameters) {
	struct sockaddr_in server_addr;
	int32 ret;
	struct sockaddr_in from;
	socklen_t fromlen;
	struct ip_info ipconfig;
	char udp_msg[DNS_LEN];
	
	memset(&ipconfig, 0, sizeof(ipconfig));
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;	   
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(53);
	server_addr.sin_len = sizeof(server_addr);
	
	do {
		sockFd=socket(AF_INET, SOCK_DGRAM, 0);
		if (sockFd==-1) {
			printf("captdns_task failed to create sock!\n");
			vTaskDelay(1000/portTICK_PERIOD_MS);
		}
	} while (sockFd==-1);
	
	do {
		ret=bind(sockFd, (struct sockaddr *)&server_addr, sizeof(server_addr));
		if (ret!=0) {
			printf("captdns_task failed to bind sock!\n");
			vTaskDelay(1000/portTICK_PERIOD_MS);
		}
	} while (ret!=0);

	printf("CaptDNS inited.\n");
	while(1) {
		memset(&from, 0, sizeof(from));
		fromlen=sizeof(struct sockaddr_in);
		ret=recvfrom(sockFd, (uint8 *)udp_msg, DNS_LEN, 0,(struct sockaddr *)&from,(socklen_t *)&fromlen);
		if (ret>0) captdnsRecv(&from,udp_msg,ret);
	}
	
	close(sockFd);
	vTaskDelete(NULL);
}

void captdnsInit(void) {
	xTaskCreate(captdnsTask, (const char *)"captdns_task", 1200, NULL, 3, NULL);
}

#else

void captdnsInit(void) {
	static struct espconn conn;
	static esp_udp udpconn;
	conn.type=ESPCONN_UDP;
	conn.proto.udp=&udpconn;
	conn.proto.udp->local_port = 53;
	espconn_regist_recvcb(&conn, captdnsRecv);
	espconn_create(&conn);
}

#endif
