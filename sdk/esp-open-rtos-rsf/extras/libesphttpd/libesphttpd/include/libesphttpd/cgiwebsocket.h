#ifndef CGIWEBSOCKET_H
#define CGIWEBSOCKET_H

#include "httpd.h"

#define WEBSOCK_FLAG_NONE 0
#define WEBSOCK_FLAG_CONT (1<<0) //Set if the data is not the final data in the message; more follows
#define WEBSOCK_FLAG_BIN (1<<1) //Set if the data is binary instead of text



typedef struct Websock Websock;
typedef struct WebsockPriv WebsockPriv;

typedef void(*WsConnectedCb)(Websock *ws);
typedef void(*WsRecvCb)(Websock *ws, char *data, int len, int flags);
typedef void(*WsSentCb)(Websock *ws);
typedef void(*WsCloseCb)(Websock *ws);

struct Websock {
	void *userData;
	HttpdConnData *conn;
	uint8_t status;
	WsRecvCb recvCb;
	WsSentCb sentCb;
	WsCloseCb closeCb;
	WebsockPriv *priv;
};

int ICACHE_FLASH_ATTR cgiWebsocket(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgiWebsocketSend(Websock *ws, char *data, int len, int flags);
void ICACHE_FLASH_ATTR cgiWebsocketClose(Websock *ws, int reason);
int ICACHE_FLASH_ATTR cgiWebSocketRecv(HttpdConnData *connData, char *data, int len);
int ICACHE_FLASH_ATTR cgiWebsockBroadcast(char *resource, char *data, int len, int flags);


#endif