/*
Connector to let httpd use the espfs filesystem to serve the files in it.
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
#include <stdlib.h>
#include <stdio.h>

#include <libesphttpd/httpdespfs.h>
#include <libesphttpd/espfs.h>
#include "espfsformat.h"

// The static files marked with FLAG_GZIP are compressed and will be served with GZIP compression.
// If the client does not advertise that he accepts GZIP send following warning message (telnet users for e.g.)
static const char *gzipNonSupportedMessage = "HTTP/1.0 501 Not implemented\r\nServer: esp8266-httpd/"HTTPDVER"\r\nConnection: close\r\nContent-Type: text/plain\r\nContent-Length: 52\r\n\r\nYour browser does not accept gzip-compressed data.\r\n";


//This is a catch-all cgi function. It takes the url passed to it, looks up the corresponding
//path in the filesystem and if it exists, passes the file through. This simulates what a normal
//webserver would do with static files.
int ICACHE_FLASH_ATTR cgiEspFsHook(HttpdConnData *connData) {
	EspFsFile *file=connData->cgiData;
	int len;
	char buff[1024];
	char acceptEncodingBuffer[64];
	int isGzip;
	
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		espFsClose(file);
		return HTTPD_CGI_DONE;
	}

	//First call to this cgi.
	if (file==NULL) {
		if (connData->cgiArg != NULL) {
			//Open a different file than provided in http request.
			//Common usage: {"/", cgiEspFsHook, "/index.html"} will show content of index.html without actual redirect to that file if host root was requested
			file = espFsOpen((char*)connData->cgiArg);
		} else {
			//Open the file so we can read it.
			file = espFsOpen(connData->url);
		}

		if (file==NULL) {
			return HTTPD_CGI_NOTFOUND;
		}

		// The gzip checking code is intentionally without #ifdefs because checking
		// for FLAG_GZIP (which indicates gzip compressed file) is very easy, doesn't
		// mean additional overhead and is actually safer to be on at all times.
		// If there are no gzipped files in the image, the code bellow will not cause any harm.

		// Check if requested file was GZIP compressed
		isGzip = espFsFlags(file) & FLAG_GZIP;
		if (isGzip) {
			// Check the browser's "Accept-Encoding" header. If the client does not
			// advertise that he accepts GZIP send a warning message (telnet users for e.g.)
			httpdGetHeader(connData, "Accept-Encoding", acceptEncodingBuffer, 64);
			if (strstr(acceptEncodingBuffer, "gzip") == NULL) {
				//No Accept-Encoding: gzip header present
				httpdSend(connData, gzipNonSupportedMessage, -1);
				espFsClose(file);
				return HTTPD_CGI_DONE;
			}
		}

		connData->cgiData=file;
		httpdStartResponse(connData, 200);
		httpdHeader(connData, "Content-Type", httpdGetMimetype(connData->url));
		if (isGzip) {
			httpdHeader(connData, "Content-Encoding", "gzip");
		}
		httpdHeader(connData, "Cache-Control", "max-age=3600, must-revalidate");
		httpdEndHeaders(connData);
		return HTTPD_CGI_MORE;
	}

	len=espFsRead(file, buff, 1024);
	if (len>0) httpdSend(connData, buff, len);
	if (len!=1024) {
		//We're done.
		espFsClose(file);
		return HTTPD_CGI_DONE;
	} else {
		//Ok, till next time.
		return HTTPD_CGI_MORE;
	}
}


//cgiEspFsTemplate can be used as a template.

typedef struct {
	EspFsFile *file;
	void *tplArg;
	char token[64];
	int tokenPos;
} TplData;

typedef void (* TplCallback)(HttpdConnData *connData, char *token, void **arg);

int ICACHE_FLASH_ATTR cgiEspFsTemplate(HttpdConnData *connData) {
	TplData *tpd=connData->cgiData;
	int len;
	int x, sp=0;
	char *e=NULL;
	char buff[1025];

	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		((TplCallback)(connData->cgiArg))(connData, NULL, &tpd->tplArg);
		espFsClose(tpd->file);
		free(tpd);
		return HTTPD_CGI_DONE;
	}

	if (tpd==NULL) {
		//First call to this cgi. Open the file so we can read it.
		tpd=(TplData *)malloc(sizeof(TplData));
		if (tpd==NULL) return HTTPD_CGI_NOTFOUND;
		tpd->file=espFsOpen(connData->url);
		tpd->tplArg=NULL;
		tpd->tokenPos=-1;
		if (tpd->file==NULL) {
			espFsClose(tpd->file);
			free(tpd);
			return HTTPD_CGI_NOTFOUND;
		}
		if (espFsFlags(tpd->file) & FLAG_GZIP) {
			printf("cgiEspFsTemplate: Trying to use gzip-compressed file %s as template!\n", connData->url);
			espFsClose(tpd->file);
			free(tpd);
			return HTTPD_CGI_NOTFOUND;
		}
		connData->cgiData=tpd;
		httpdStartResponse(connData, 200);
		httpdHeader(connData, "Content-Type", httpdGetMimetype(connData->url));
		httpdEndHeaders(connData);
		return HTTPD_CGI_MORE;
	}

	len=espFsRead(tpd->file, buff, 1024);
	if (len>0) {
		sp=0;
		e=buff;
		for (x=0; x<len; x++) {
			if (tpd->tokenPos==-1) {
				//Inside ordinary text.
				if (buff[x]=='%') {
					//Send raw data up to now
					if (sp!=0) httpdSend(connData, e, sp);
					sp=0;
					//Go collect token chars.
					tpd->tokenPos=0;
				} else {
					sp++;
				}
			} else {
				if (buff[x]=='%') {
					if (tpd->tokenPos==0) {
						//This is the second % of a %% escape string.
						//Send a single % and resume with the normal program flow.
						httpdSend(connData, "%", 1);
					} else {
						//This is an actual token.
						tpd->token[tpd->tokenPos++]=0; //zero-terminate token
						((TplCallback)(connData->cgiArg))(connData, tpd->token, &tpd->tplArg);
					}
					//Go collect normal chars again.
					e=&buff[x+1];
					tpd->tokenPos=-1;
				} else {
					if (tpd->tokenPos<(sizeof(tpd->token)-1)) tpd->token[tpd->tokenPos++]=buff[x];
				}
			}
		}
	}
	//Send remaining bit.
	if (sp!=0) httpdSend(connData, e, sp);
	if (len!=1024) {
		//We're done.
		((TplCallback)(connData->cgiArg))(connData, NULL, &tpd->tplArg);
		espFsClose(tpd->file);
		free(tpd);
		return HTTPD_CGI_DONE;
	} else {
		//Ok, till next time.
		return HTTPD_CGI_MORE;
	}
}

