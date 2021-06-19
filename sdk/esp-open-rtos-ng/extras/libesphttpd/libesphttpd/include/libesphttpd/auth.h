#ifndef AUTH_H
#define AUTH_H

#include "httpd.h"

#ifndef HTTP_AUTH_REALM
#define HTTP_AUTH_REALM "Protected"
#endif

#define HTTPD_AUTH_SINGLE 0
#define HTTPD_AUTH_CALLBACK 1

#define AUTH_MAX_USER_LEN 32
#define AUTH_MAX_PASS_LEN 32

//Parameter given to authWhatever functions. This callback returns the usernames/passwords the device
//has.
typedef int (* AuthGetUserPw)(HttpdConnData *connData, int no, char *user, int userLen, char *pass, int passLen);

int ICACHE_FLASH_ATTR authBasic(HttpdConnData *connData);

#endif