/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */

/*
This is example code for the esphttpd library. It's a small-ish demo showing off 
the server, including WiFi connection management capabilities, some IO and
some pictures of cats.
*/

#include <string.h>
#include <stdio.h>

#include <espressif/esp_common.h>
#include <etstimer.h>
#include <libesphttpd/httpd.h>
#include <libesphttpd/httpdespfs.h>
#include <libesphttpd/cgiwifi.h>
#include <libesphttpd/cgiflash.h>
#include <libesphttpd/auth.h>
#include <libesphttpd/espfs.h>
#include <libesphttpd/captdns.h>
#include <libesphttpd/webpages-espfs.h>
#include <libesphttpd/cgiwebsocket.h>
#include <dhcpserver.h>

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>
#include <esp/uart.h>

#include "io.h"
#include "cgi.h"
#include "cgi-test.h"

#define AP_SSID "esp-open-rtos AP"
#define AP_PSK "esp-open-rtos"

//Function that tells the authentication system what users/passwords live on the system.
//This is disabled in the default build; if you want to try it, enable the authBasic line in
//the builtInUrls below.
int myPassFn(HttpdConnData *connData, int no, char *user, int userLen, char *pass, int passLen) {
	if (no==0) {
		strcpy(user, "admin");
		strcpy(pass, "s3cr3t");
		return 1;
//Add more users this way. Check against incrementing no for each user added.
//	} else if (no==1) {
//		strcpy(user, "user1");
//		strcpy(pass, "something");
//		return 1;
	}
	return 0;
}


static ETSTimer websockTimer;

//Broadcast the uptime in seconds every second over connected websockets
static void websocketBcast(void *arg) {
	static int ctr=0;
	char buff[128];
	while(1) {
		ctr++;
		sprintf(buff, "Up for %d minutes %d seconds!\n", ctr/60, ctr%60);
		cgiWebsockBroadcast("/websocket/ws.cgi", buff, strlen(buff), WEBSOCK_FLAG_NONE);
		vTaskDelay(1000/portTICK_PERIOD_MS);
	}
}

//On reception of a message, send "You sent: " plus whatever the other side sent
static void myWebsocketRecv(Websock *ws, char *data, int len, int flags) {
	int i;
	char buff[128];
	sprintf(buff, "You sent: ");
	for (i=0; i<len; i++) buff[i+10]=data[i];
	buff[i+10]=0;
	cgiWebsocketSend(ws, buff, strlen(buff), WEBSOCK_FLAG_NONE);
}

//Websocket connected. Install reception handler and send welcome message.
static void myWebsocketConnect(Websock *ws) {
	ws->recvCb=myWebsocketRecv;
	cgiWebsocketSend(ws, "Hi, Websocket!", 14, WEBSOCK_FLAG_NONE);
}

//On reception of a message, echo it back verbatim
void myEchoWebsocketRecv(Websock *ws, char *data, int len, int flags) {
	printf("EchoWs: echo, len=%d\n", len);
	cgiWebsocketSend(ws, data, len, flags);
}

//Echo websocket connected. Install reception handler.
void myEchoWebsocketConnect(Websock *ws) {
	printf("EchoWs: connect\n");
	ws->recvCb=myEchoWebsocketRecv;
}

CgiUploadFlashDef uploadParams={
	.type=CGIFLASH_TYPE_FW,
	.fw1Pos=0x2000,
	.fw2Pos=((FLASH_SIZE*1024*1024)/2)+0x2000,
	.fwSize=((FLASH_SIZE*1024*1024)/2)-0x2000,
	.tagName=LIBESPHTTPD_OTA_TAGNAME
};


/*
This is the main url->function dispatching data struct.
In short, it's a struct with various URLs plus their handlers. The handlers can
be 'standard' CGI functions you wrote, or 'special' CGIs requiring an argument.
They can also be auth-functions. An asterisk will match any url starting with
everything before the asterisks; "*" matches everything. The list will be
handled top-down, so make sure to put more specific rules above the more
general ones. Authorization things (like authBasic) act as a 'barrier' and
should be placed above the URLs they protect.
*/
HttpdBuiltInUrl builtInUrls[]={
	{"*", cgiRedirectApClientToHostname, "esp8266.nonet"},
	{"/", cgiRedirect, "/index.tpl"},
	{"/led.tpl", cgiEspFsTemplate, tplLed},
	{"/index.tpl", cgiEspFsTemplate, tplCounter},
	{"/led.cgi", cgiLed, NULL},
#ifndef ESP32
	{"/flash/", cgiRedirect, "/flash/index.html"},
	{"/flash/next", cgiGetFirmwareNext, &uploadParams},
	{"/flash/upload", cgiUploadFirmware, &uploadParams},
	{"/flash/reboot", cgiRebootFirmware, NULL},
#endif
	//Routines to make the /wifi URL and everything beneath it work.

//Enable the line below to protect the WiFi configuration with an username/password combo.
//	{"/wifi/*", authBasic, myPassFn},

	{"/wifi", cgiRedirect, "/wifi/wifi.tpl"},
	{"/wifi/", cgiRedirect, "/wifi/wifi.tpl"},
	{"/wifi/wifiscan.cgi", cgiWiFiScan, NULL},
	{"/wifi/wifi.tpl", cgiEspFsTemplate, tplWlan},
	{"/wifi/connect.cgi", cgiWiFiConnect, NULL},
	{"/wifi/connstatus.cgi", cgiWiFiConnStatus, NULL},
	{"/wifi/setmode.cgi", cgiWiFiSetMode, NULL},

	{"/websocket/ws.cgi", cgiWebsocket, myWebsocketConnect},
	{"/websocket/echo.cgi", cgiWebsocket, myEchoWebsocketConnect},

	{"/test", cgiRedirect, "/test/index.html"},
	{"/test/", cgiRedirect, "/test/index.html"},
	{"/test/test.cgi", cgiTestbed, NULL},

	{"*", cgiEspFsHook, NULL}, //Catch-all cgi function for the filesystem
	{NULL, NULL, NULL}
};

void wifiInit() {
    struct ip_info ap_ip;
    uint8_t sdk_wifi_get_opmode();
    switch(sdk_wifi_get_opmode()) {
        case STATIONAP_MODE:
        case SOFTAP_MODE:
            IP4_ADDR(&ap_ip.ip, 172, 16, 0, 1);
            IP4_ADDR(&ap_ip.gw, 0, 0, 0, 0);
            IP4_ADDR(&ap_ip.netmask, 255, 255, 0, 0);
            sdk_wifi_set_ip_info(1, &ap_ip);

            struct sdk_softap_config ap_config = {
                .ssid = AP_SSID,
                .ssid_hidden = 0,
                .channel = 3,
                .ssid_len = strlen(AP_SSID),
                .authmode = AUTH_WPA_WPA2_PSK,
                .password = AP_PSK,
                .max_connection = 3,
                .beacon_interval = 100,
            };
            sdk_wifi_softap_set_config(&ap_config);

            ip_addr_t first_client_ip;
            IP4_ADDR(&first_client_ip, 172, 16, 0, 2);
            dhcpserver_start(&first_client_ip, 4);
            dhcpserver_set_dns(&ap_ip.ip);
            dhcpserver_set_router(&ap_ip.ip);
            break;
        case STATION_MODE:
            break;
        default:
            break;
    }
}

//Main routine. Initialize stdout, the I/O, filesystem and the webserver and we're done.
void user_init(void) {
    uart_set_baud(0, 115200);

	wifiInit();
	ioInit();
	captdnsInit();

	espFsInit((void*)(_binary_build_web_espfs_bin_start));
	httpdInit(builtInUrls, 80);

	xTaskCreate(websocketBcast, "wsbcast", 384, NULL, 3, NULL);

	printf("\nReady\n");
}
