/*
Cgi routines as used by the tests in the html/test subdirectory.
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

#include <espressif/esp_common.h>

#include "cgi-test.h"

typedef struct {
	int len;
	int sendPos;
} TestbedState;


int ICACHE_FLASH_ATTR cgiTestbed(HttpdConnData *connData) {
	char buff[1024];
	int first=0;
	int l, x;
	TestbedState *state=(TestbedState*)connData->cgiData;

	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		if (state) free(state);
		return HTTPD_CGI_DONE;
	}

	if (state==NULL) {
		//First call
		state=malloc(sizeof(TestbedState));
		memset(state, 0, sizeof(state));
		connData->cgiData=state;
		first=1;
	}

	if (connData->requestType==HTTPD_METHOD_GET) {
		if (first) {
			httpdStartResponse(connData, 200);
			httpdHeader(connData, "content-type", "application/data");
			httpdEndHeaders(connData);
			l=httpdFindArg(connData->getArgs, "len", buff, sizeof(buff));
			state->len=1024;
			if (l!=-1) state->len=atoi(buff);
			state->sendPos=0;
			return HTTPD_CGI_MORE;
		} else {
			l=sizeof(buff);
			if (l>(state->len-state->sendPos)) l=(state->len-state->sendPos);
			//Fill with semi-random data
			for (x=0; x<l; x++) buff[x]=((x^(state->sendPos>>10))&0x1F)+'0';
			httpdSend(connData, buff, l);
			state->sendPos+=l;
			printf("Test: Uploaded %d/%d bytes\n", state->sendPos, state->len);
			if (state->len<=state->sendPos) {
				if (state) free(state);
				return HTTPD_CGI_DONE; 
			} else {
				return HTTPD_CGI_MORE;
			}
		}
	}
	if (connData->requestType==HTTPD_METHOD_POST) {
		if (connData->post->len!=connData->post->received) {
			//Still receiving data. Ignore this.
			printf("Test: got %d/%d bytes\n", connData->post->received, connData->post->len);
			return HTTPD_CGI_MORE;
		} else {
			httpdStartResponse(connData, 200);
			httpdHeader(connData, "content-type", "text/plain");
			httpdEndHeaders(connData);
			l=sprintf(buff, "%d", connData->post->received);
			httpdSend(connData, buff, l);
			return HTTPD_CGI_DONE;
		}
	}
	return HTTPD_CGI_DONE;
}
