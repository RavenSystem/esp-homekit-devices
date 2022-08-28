/*
Cgi/template routines for the /wifi url.
*/

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <etstimer.h>
#include <espressif/osapi.h>
#include <espressif/esp_timer.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_misc.h>
#include <espressif/esp_system.h>

#include <libesphttpd/platform.h>
#include <libesphttpd/cgiwifi.h>

//Enable this to disallow any changes in AP settings
//#define DEMO_MODE

//WiFi access point data
typedef struct {
	char ssid[32];
	char bssid[8];
	int channel;
	char rssi;
	char enc;
} ApData;

//Scan result
typedef struct {
	char scanInProgress; //if 1, don't access the underlying stuff from the webpage.
	ApData **apData;
	int noAps;
} ScanResultData;

//Static scan status storage.
static ScanResultData cgiWifiAps;

#define CONNTRY_IDLE 0
#define CONNTRY_WORKING 1
#define CONNTRY_SUCCESS 2
#define CONNTRY_FAIL 3
//Connection result var
static int connTryStatus=CONNTRY_IDLE;
static ETSTimer resetTimer;

//Callback the code calls when a wlan ap scan is done. Basically stores the result in
//the cgiWifiAps struct.
void wifiScanDoneCb(void *arg, sdk_scan_status_t status) {
	int n;
	struct sdk_bss_info *bss_link = (struct sdk_bss_info *)arg;
	printf("wifiScanDoneCb %d\n", status);
	if (status!=SCAN_OK) {
		cgiWifiAps.scanInProgress=0;
		return;
	}

	//Clear prev ap data if needed.
	if (cgiWifiAps.apData!=NULL) {
		for (n=0; n<cgiWifiAps.noAps; n++) free(cgiWifiAps.apData[n]);
		free(cgiWifiAps.apData);
	}

	//Count amount of access points found.
	n=0;
	while (bss_link != NULL) {
		bss_link = bss_link->next.stqe_next;
		n++;
	}
	//Allocate memory for access point data
	cgiWifiAps.apData=(ApData **)malloc(sizeof(ApData *)*n);
	if (cgiWifiAps.apData==NULL) {
		printf("Out of memory allocating apData\n");
		return;
	}
	cgiWifiAps.noAps=n;
	printf("Scan done: found %d APs\n", n);

	//Copy access point data to the static struct
	n=0;
	bss_link = (struct sdk_bss_info *)arg;
	while (bss_link != NULL) {
		if (n>=cgiWifiAps.noAps) {
			//This means the bss_link changed under our nose. Shouldn't happen!
			//Break because otherwise we will write in unallocated memory.
			printf("Huh? I have more than the allocated %d aps!\n", cgiWifiAps.noAps);
			break;
		}
		//Save the ap data.
		cgiWifiAps.apData[n]=(ApData *)malloc(sizeof(ApData));
		if (cgiWifiAps.apData[n]==NULL) {
			printf("Can't allocate mem for ap buff.\n");
			cgiWifiAps.scanInProgress=0;
			return;
		}
		cgiWifiAps.apData[n]->rssi=bss_link->rssi;
		cgiWifiAps.apData[n]->channel=bss_link->channel;
		cgiWifiAps.apData[n]->enc=bss_link->authmode;
		strncpy(cgiWifiAps.apData[n]->ssid, (char*)bss_link->ssid, 32);
		strncpy(cgiWifiAps.apData[n]->bssid, (char*)bss_link->bssid, 6);

		bss_link = bss_link->next.stqe_next;
		n++;
	}
	//We're done.
	cgiWifiAps.scanInProgress=0;
}


//Routine to start a WiFi access point scan.
static void wifiStartScan() {
//	int x;
	if (cgiWifiAps.scanInProgress) return;
	cgiWifiAps.scanInProgress=1;
	sdk_wifi_station_scan(NULL, (sdk_scan_done_cb_t)&wifiScanDoneCb);
}

//This CGI is called from the bit of AJAX-code in wifi.tpl. It will initiate a
//scan for access points and if available will return the result of an earlier scan.
//The result is embedded in a bit of JSON parsed by the javascript in wifi.tpl.
int cgiWiFiScan(HttpdConnData *connData) {
	int pos=(int)connData->cgiData;
	int len;
	char buff[1024];

	if (!cgiWifiAps.scanInProgress && pos!=0) {
		//Fill in json code for an access point
		if (pos-1<cgiWifiAps.noAps) {
			len=sprintf(buff, "{\"essid\": \"%s\", \"bssid\": \"" MACSTR "\", \"rssi\": \"%d\", \"enc\": \"%d\", \"channel\": \"%d\"}%s\n",
					cgiWifiAps.apData[pos-1]->ssid, MAC2STR(cgiWifiAps.apData[pos-1]->bssid), cgiWifiAps.apData[pos-1]->rssi,
					cgiWifiAps.apData[pos-1]->enc, cgiWifiAps.apData[pos-1]->channel, (pos-1==cgiWifiAps.noAps-1)?"":",");
			httpdSend(connData, buff, len);
		}
		pos++;
		if ((pos-1)>=cgiWifiAps.noAps) {
			len=sprintf(buff, "]\n}\n}\n");
			httpdSend(connData, buff, len);
			//Also start a new scan.
			wifiStartScan();
			return HTTPD_CGI_DONE;
		} else {
			connData->cgiData=(void*)pos;
			return HTTPD_CGI_MORE;
		}
	}

	httpdStartResponse(connData, 200);
	httpdHeader(connData, "Content-Type", "text/json");
	httpdEndHeaders(connData);

	if (cgiWifiAps.scanInProgress==1) {
		//We're still scanning. Tell Javascript code that.
		len=sprintf(buff, "{\n \"result\": { \n\"inProgress\": \"1\"\n }\n}\n");
		httpdSend(connData, buff, len);
		return HTTPD_CGI_DONE;
	} else {
		//We have a scan result. Pass it on.
		len=sprintf(buff, "{\n \"result\": { \n\"inProgress\": \"0\", \n\"APs\": [\n");
		httpdSend(connData, buff, len);
		if (cgiWifiAps.apData==NULL) cgiWifiAps.noAps=0;
		connData->cgiData=(void *)1;
		return HTTPD_CGI_MORE;
	}
}

//Temp store for new ap info.
static struct sdk_station_config stconf;

//This routine is ran some time after a connection attempt to an access point. If
//the connect succeeds, this gets the module in STA-only mode.
static void resetTimerCb(void *arg) {
	int x=sdk_wifi_station_get_connect_status();
	if (x==STATION_GOT_IP) {
		//Go to STA mode. This needs a reset, so do that.
		printf("Got IP. Going into STA mode..\n");
		sdk_wifi_set_opmode(1);
		sdk_system_restart();
	} else {
		connTryStatus=CONNTRY_FAIL;
		printf("Connect fail. Not going into STA-only mode.\n");
		//Maybe also pass this through on the webpage?
	}
}



//Actually connect to a station. This routine is timed because I had problems
//with immediate connections earlier. It probably was something else that caused it,
//but I can't be arsed to put the code back :P
static void reassTimerCb(void *arg) {
	int x;
	printf("Try to connect to AP....\n");
	sdk_wifi_station_disconnect();
	sdk_wifi_station_set_config(&stconf);
	sdk_wifi_station_connect();
	x=sdk_wifi_get_opmode();
	connTryStatus=CONNTRY_WORKING;
	if (x!=1) {
		//Schedule disconnect/connect
		sdk_os_timer_disarm(&resetTimer);
		sdk_os_timer_setfn(&resetTimer, resetTimerCb, NULL);
		sdk_os_timer_arm(&resetTimer, 15000, 0); //time out after 15 secs of trying to connect
	}
}


//This cgi uses the routines above to connect to a specific access point with the
//given ESSID using the given password.
int cgiWiFiConnect(HttpdConnData *connData) {
	char essid[128];
	char passwd[128];
	static ETSTimer reassTimer;
	
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}
	
	httpdFindArg(connData->post->buff, "essid", essid, sizeof(essid));
	httpdFindArg(connData->post->buff, "passwd", passwd, sizeof(passwd));

	strncpy((char*)stconf.ssid, essid, 32);
	strncpy((char*)stconf.password, passwd, 64);
	printf("Try to connect to AP %s pw %s\n", essid, passwd);

	//Schedule disconnect/connect
	sdk_os_timer_disarm(&reassTimer);
	sdk_os_timer_setfn(&reassTimer, reassTimerCb, NULL);
//Set to 0 if you want to disable the actual reconnecting bit
#ifdef DEMO_MODE
	httpdRedirect(connData, "/wifi");
#else
	sdk_os_timer_arm(&reassTimer, 500, 0);
	httpdRedirect(connData, "connecting.html");
#endif
	return HTTPD_CGI_DONE;
}

//This cgi uses the routines above to connect to a specific access point with the
//given ESSID using the given password.
int cgiWiFiSetMode(HttpdConnData *connData) {
	int len;
	char buff[1024];
	
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	len=httpdFindArg(connData->getArgs, "mode", buff, sizeof(buff));
	if (len!=0) {
		printf("cgiWifiSetMode: %s\n", buff);
#ifndef DEMO_MODE
		sdk_wifi_set_opmode(atoi(buff));
		sdk_system_restart();
#endif
	}
	httpdRedirect(connData, "/wifi");
	return HTTPD_CGI_DONE;
}

int cgiWiFiConnStatus(HttpdConnData *connData) {
	char buff[1024];
	int len;
	struct ip_info info;
	int st=sdk_wifi_station_get_connect_status();
	httpdStartResponse(connData, 200);
	httpdHeader(connData, "Content-Type", "text/json");
	httpdEndHeaders(connData);
	if (connTryStatus==CONNTRY_IDLE) {
		len=sprintf(buff, "{\n \"status\": \"idle\"\n }\n");
	} else if (connTryStatus==CONNTRY_WORKING || connTryStatus==CONNTRY_SUCCESS) {
		if (st==STATION_GOT_IP) {
			sdk_wifi_get_ip_info(0, &info);
			len=sprintf(buff, "{\n \"status\": \"success\",\n \"ip\": \"%d.%d.%d.%d\" }\n", 
				(info.ip.addr>>0)&0xff, (info.ip.addr>>8)&0xff, 
				(info.ip.addr>>16)&0xff, (info.ip.addr>>24)&0xff);
			//Reset into AP-only mode sooner.
			sdk_os_timer_disarm(&resetTimer);
			sdk_os_timer_setfn(&resetTimer, resetTimerCb, NULL);
			sdk_os_timer_arm(&resetTimer, 1000, 0);
		} else {
			len=sprintf(buff, "{\n \"status\": \"working\"\n }\n");
		}
	} else {
		len=sprintf(buff, "{\n \"status\": \"fail\"\n }\n");
	}

	httpdSend(connData, buff, len);
	return HTTPD_CGI_DONE;
}

//Template code for the WLAN page.
int tplWlan(HttpdConnData *connData, char *token, void **arg) {
	char buff[1024];
	int x;
	static struct sdk_station_config stconf;
	if (token==NULL) return HTTPD_CGI_DONE;
	sdk_wifi_station_get_config(&stconf);

	strcpy(buff, "Unknown");
	if (strcmp(token, "WiFiMode")==0) {
		x=sdk_wifi_get_opmode();
		if (x==1) strcpy(buff, "Client");
		if (x==2) strcpy(buff, "SoftAP");
		if (x==3) strcpy(buff, "STA+AP");
	} else if (strcmp(token, "currSsid")==0) {
		strcpy(buff, (char*)stconf.ssid);
	} else if (strcmp(token, "WiFiPasswd")==0) {
		strcpy(buff, (char*)stconf.password);
	} else if (strcmp(token, "WiFiapwarn")==0) {
		x=sdk_wifi_get_opmode();
		if (x==2) {
			strcpy(buff, "<b>Can't scan in this mode.</b> Click <a href=\"setmode.cgi?mode=3\">here</a> to go to STA+AP mode.");
		} else {
			strcpy(buff, "Click <a href=\"setmode.cgi?mode=2\">here</a> to go to standalone AP mode.");
		}
	}
	httpdSend(connData, buff, -1);
	return HTTPD_CGI_DONE;
}

