/*
ESP8266 web server - platform-dependent routines, FreeRTOS version


Thanks to my collague at Espressif for writing the foundations of this code.
*/
#ifdef FREERTOS

#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <esp8266.h>
#include <libesphttpd/httpd.h>
#include <libesphttpd/platform.h>
#include "httpd-platform.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "lwip/sockets.h"


static int httpPort;
static int httpMaxConnCt;
static QueueHandle_t httpdMux;


struct  RtosConnType{
	int fd;
	int needWriteDoneNotif;
	int needsClose;
	int port;
	char ip[4];
};

static RtosConnType rconn[HTTPD_MAX_CONNECTIONS];

int ICACHE_FLASH_ATTR httpdPlatSendData(ConnTypePtr conn, char *buff, int len) {
	conn->needWriteDoneNotif=1;
	return (write(conn->fd, buff, len)>=0);
}

void ICACHE_FLASH_ATTR httpdPlatDisconnect(ConnTypePtr conn) {
	conn->needsClose=1;
	conn->needWriteDoneNotif=1; //because the real close is done in the writable select code
}

void httpdPlatDisableTimeout(ConnTypePtr conn) {
	//Unimplemented for FreeRTOS
}

//Set/clear global httpd lock.
void ICACHE_FLASH_ATTR httpdPlatLock() {
	xSemaphoreTakeRecursive(httpdMux, portMAX_DELAY);
}

void ICACHE_FLASH_ATTR httpdPlatUnlock() {
	xSemaphoreGiveRecursive(httpdMux);
}


#define RECV_BUF_SIZE 2048
static void platHttpServerTask(void *pvParameters) {
	int32 listenfd;
	int32 remotefd;
	int32 len;
	int32 ret;
	int x;
	int maxfdp = 0;
	char *precvbuf;
	fd_set readset,writeset;
	struct sockaddr name;
	//struct timeval timeout;
	struct sockaddr_in server_addr;
	struct sockaddr_in remote_addr;
	
	httpdMux=xSemaphoreCreateRecursiveMutex();
	
	for (x=0; x<HTTPD_MAX_CONNECTIONS; x++) {
		rconn[x].fd=-1;
	}
	
	/* Construct local address structure */
	memset(&server_addr, 0, sizeof(server_addr)); /* Zero out structure */
	server_addr.sin_family = AF_INET;			/* Internet address family */
	server_addr.sin_addr.s_addr = INADDR_ANY;   /* Any incoming interface */
	server_addr.sin_len = sizeof(server_addr);  
	server_addr.sin_port = htons(httpPort); /* Local port */

	/* Create socket for incoming connections */
	do{
		listenfd = socket(AF_INET, SOCK_STREAM, 0);
		if (listenfd == -1) {
			printf("platHttpServerTask: failed to create sock!\n");
			vTaskDelay(1000/portTICK_PERIOD_MS);
		}
	} while(listenfd == -1);

	/* Bind to the local port */
	do{
		ret = bind(listenfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
		if (ret != 0) {
			printf("platHttpServerTask: failed to bind!\n");
			vTaskDelay(1000/portTICK_PERIOD_MS);
		}
	} while(ret != 0);

	do{
		/* Listen to the local connection */
		ret = listen(listenfd, HTTPD_MAX_CONNECTIONS);
		if (ret != 0) {
			printf("platHttpServerTask: failed to listen!\n");
			vTaskDelay(1000/portTICK_PERIOD_MS);
		}
		
	} while(ret != 0);
	
	printf("esphttpd: active and listening to connections.\n");
	while(1){
		// clear fdset, and set the select function wait time
		int socketsFull=1;
		maxfdp = 0;
		FD_ZERO(&readset);
		FD_ZERO(&writeset);
		//timeout.tv_sec = 2;
		//timeout.tv_usec = 0;
		
		for(x=0; x<HTTPD_MAX_CONNECTIONS; x++){
			if (rconn[x].fd!=-1) {
				FD_SET(rconn[x].fd, &readset);
				if (rconn[x].needWriteDoneNotif) FD_SET(rconn[x].fd, &writeset);
				if (rconn[x].fd>maxfdp) maxfdp=rconn[x].fd;
			} else {
				socketsFull=0;
			}
		}
		
		if (!socketsFull) {
			FD_SET(listenfd, &readset);
			if (listenfd>maxfdp) maxfdp=listenfd;
		}

		//polling all exist client handle,wait until readable/writable
		ret = select(maxfdp+1, &readset, &writeset, NULL, NULL);//&timeout
		if(ret > 0){
			//See if we need to accept a new connection
			if (FD_ISSET(listenfd, &readset)) {
				len=sizeof(struct sockaddr_in);
				remotefd = accept(listenfd, (struct sockaddr *)&remote_addr, (socklen_t *)&len);
				if (remotefd<0) {
					printf("platHttpServerTask: Huh? Accept failed: %ld.\n", remotefd);
					continue;
				}
				for(x=0; x<HTTPD_MAX_CONNECTIONS; x++) if (rconn[x].fd==-1) break;
				if (x==HTTPD_MAX_CONNECTIONS) {
					printf("platHttpServerTask: Huh? Got accept with all slots full.\n");
					continue;
				}
				int keepAlive = 1; //enable keepalive
				int keepIdle = 60; //60s
				int keepInterval = 5; //5s
				int keepCount = 3; //retry times
					
				setsockopt(remotefd, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepAlive, sizeof(keepAlive));
				setsockopt(remotefd, IPPROTO_TCP, TCP_KEEPIDLE, (void*)&keepIdle, sizeof(keepIdle));
				setsockopt(remotefd, IPPROTO_TCP, TCP_KEEPINTVL, (void *)&keepInterval, sizeof(keepInterval));
				setsockopt(remotefd, IPPROTO_TCP, TCP_KEEPCNT, (void *)&keepCount, sizeof(keepCount));
				
				rconn[x].fd=remotefd;
				rconn[x].needWriteDoneNotif=0;
				rconn[x].needsClose=0;
				
				len=sizeof(name);
				getpeername(remotefd, &name, (socklen_t *)&len);
				struct sockaddr_in *piname=(struct sockaddr_in *)&name;

				rconn[x].port=piname->sin_port;
				memcpy(&rconn[x].ip, &piname->sin_addr.s_addr, sizeof(rconn[x].ip));

				httpdConnectCb(&rconn[x], rconn[x].ip, rconn[x].port);
				//os_timer_disarm(&connData[x].conn->stop_watch);
				//os_timer_setfn(&connData[x].conn->stop_watch, (os_timer_func_t *)httpserver_conn_watcher, connData[x].conn);
				//os_timer_arm(&connData[x].conn->stop_watch, STOP_TIMER, 0);
//				printf("httpserver acpt index %d sockfd %d!\n", x, remotefd);
			}
			
			//See if anything happened on the existing connections.
			for(x=0; x < HTTPD_MAX_CONNECTIONS; x++){
				//Skip empty slots
				if (rconn[x].fd==-1) continue;

				//Check for write availability first: the read routines may write needWriteDoneNotif while
				//the select didn't check for that.
				if (rconn[x].needWriteDoneNotif && FD_ISSET(rconn[x].fd, &writeset)) {
					rconn[x].needWriteDoneNotif=0; //Do this first, httpdSentCb may write something making this 1 again.
					if (rconn[x].needsClose) {
						//Do callback and close fd.
						httpdDisconCb(&rconn[x], rconn[x].ip, rconn[x].port);
						close(rconn[x].fd);
						rconn[x].fd=-1;
					} else {
						httpdSentCb(&rconn[x], rconn[x].ip, rconn[x].port);
					}
				}

				if (FD_ISSET(rconn[x].fd, &readset)) {
					precvbuf=(char*)malloc(RECV_BUF_SIZE);
					if (precvbuf==NULL) {
						printf("platHttpServerTask: memory exhausted!\n");
						httpdDisconCb(&rconn[x], rconn[x].ip, rconn[x].port);
						close(rconn[x].fd);
						rconn[x].fd=-1;
					}
					ret=recv(rconn[x].fd, precvbuf, RECV_BUF_SIZE,0);
					if (ret > 0) {
						//Data received. Pass to httpd.
						httpdRecvCb(&rconn[x], rconn[x].ip, rconn[x].port, precvbuf, ret);
					} else {
						//recv error,connection close
						httpdDisconCb(&rconn[x], rconn[x].ip, rconn[x].port);
						close(rconn[x].fd);
						rconn[x].fd=-1;
					}
					if (precvbuf) free(precvbuf);
				}
			}
		}
	}

#if 0
//Deinit code, not used here.
	/*release data connection*/
	for(x=0; x < HTTPD_MAX_CONNECTIONS; x++){
		//find all valid handle 
		if(connData[x].conn == NULL) continue;
		if(connData[x].conn->sockfd >= 0){
			os_timer_disarm((os_timer_t *)&connData[x].conn->stop_watch);
			close(connData[x].conn->sockfd);
			connData[x].conn->sockfd = -1;
			connData[x].conn = NULL;
			if(connData[x].cgi!=NULL) connData[x].cgi(&connData[x]); //flush cgi data
			httpdRetireConn(&connData[x]);
		}
	}
	/*release listen socket*/
	close(listenfd);

	vTaskDelete(NULL);
#endif
}



//Initialize listening socket, do general initialization
void ICACHE_FLASH_ATTR httpdPlatInit(int port, int maxConnCt) {
	httpPort=port;
	httpMaxConnCt=maxConnCt;
	xTaskCreate(platHttpServerTask, (const char *)"esphttpd", HTTPD_STACKSIZE, NULL, 4, NULL);
}


#endif
