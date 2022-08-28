#ifndef HTTPD_H
#define HTTPD_H

#define HTTPDVER "0.4"

#include <stdint.h>
#include "platform.h"

//Max length of request head. This is statically allocated for each connection.
#define HTTPD_MAX_HEAD_LEN		1024
//Max post buffer len. This is dynamically malloc'ed if needed.
#define HTTPD_MAX_POST_LEN		2048
//Max send buffer len. This is allocated on the stack.
#define HTTPD_MAX_SENDBUFF_LEN	2048
//If some data can't be sent because the underlaying socket doesn't accept the data (like the nonos
//layer is prone to do), we put it in a backlog that is dynamically malloc'ed. This defines the max
//size of the backlog.
#define HTTPD_MAX_BACKLOG_SIZE	(4*1024)

#define HTTPD_CGI_MORE 0
#define HTTPD_CGI_DONE 1
#define HTTPD_CGI_NOTFOUND 2
#define HTTPD_CGI_AUTHENTICATED 3

#define HTTPD_METHOD_GET 1
#define HTTPD_METHOD_POST 2

#define HTTPD_TRANSFER_CLOSE 0
#define HTTPD_TRANSFER_CHUNKED 1
#define HTTPD_TRANSFER_NONE 2

typedef struct HttpdPriv HttpdPriv;
typedef struct HttpdConnData HttpdConnData;
typedef struct HttpdPostData HttpdPostData;

typedef int (* cgiSendCallback)(HttpdConnData *connData);
typedef int (* cgiRecvHandler)(HttpdConnData *connData, char *data, int len);

//A struct describing a http connection. This gets passed to cgi functions.
struct HttpdConnData {
	ConnTypePtr conn;		// The TCP connection. Exact type depends on the platform.
	char requestType;		// One of the HTTPD_METHOD_* values
	char *url;				// The URL requested, without hostname or GET arguments
	char *getArgs;			// The GET arguments for this request, if any.
	const void *cgiArg;		// Argument to the CGI function, as stated as the 3rd argument of
							// the builtInUrls entry that referred to the CGI function.
	void *cgiData;			// Opaque data pointer for the CGI function
	char *hostName;			// Host name field of request
	HttpdPriv *priv;		// Opaque pointer to data for internal httpd housekeeping
	cgiSendCallback cgi;	// CGI function pointer
	cgiRecvHandler recvHdl;	// Handler for data received after headers, if any
	HttpdPostData *post;	// POST data structure
	int remote_port;		// Remote TCP port
	uint8 remote_ip[4];		// IP address of client
	uint8 slot;				// Slot ID
};

//A struct describing the POST data sent inside the http connection.  This is used by the CGI functions
struct HttpdPostData {
	int len;				// POST Content-Length
	int buffSize;			// The maximum length of the post buffer
	int buffLen;			// The amount of bytes in the current post buffer
	int received;			// The total amount of bytes received so far
	char *buff;				// Actual POST data buffer
	char *multipartBoundary; //Text of the multipart boundary, if any
};

//A struct describing an url. This is the main struct that's used to send different URL requests to
//different routines.
typedef struct {
	const char *url;
	cgiSendCallback cgiCb;
	const void *cgiArg;
} HttpdBuiltInUrl;

int cgiRedirect(HttpdConnData *connData);
int cgiRedirectToHostname(HttpdConnData *connData);
int cgiRedirectApClientToHostname(HttpdConnData *connData);
void httpdRedirect(HttpdConnData *conn, char *newUrl);
int httpdUrlDecode(char *val, int valLen, char *ret, int retLen);
int httpdFindArg(char *line, char *arg, char *buff, int buffLen);
void httpdInit(HttpdBuiltInUrl *fixedUrls, int port);
const char *httpdGetMimetype(char *url);
void httdSetTransferMode(HttpdConnData *conn, int mode);
void httpdStartResponse(HttpdConnData *conn, int code);
void httpdHeader(HttpdConnData *conn, const char *field, const char *val);
void httpdEndHeaders(HttpdConnData *conn);
int httpdGetHeader(HttpdConnData *conn, char *header, char *ret, int retLen);
int httpdSend(HttpdConnData *conn, const char *data, int len);
void httpdFlushSendBuffer(HttpdConnData *conn);
void httpdContinue(HttpdConnData *conn);
void httpdConnSendStart(HttpdConnData *conn);
void httpdConnSendFinish(HttpdConnData *conn);

//Platform dependent code should call these.
void httpdSentCb(ConnTypePtr conn, char *remIp, int remPort);
void httpdRecvCb(ConnTypePtr conn, char *remIp, int remPort, char *data, unsigned short len);
void httpdDisconCb(ConnTypePtr conn, char *remIp, int remPort);
int httpdConnectCb(ConnTypePtr conn, char *remIp, int remPort);


#endif
