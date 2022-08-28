#ifndef HTTPD_PLATFORM_H
#define HTTPD_PLATFORM_H

int httpdPlatSendData(ConnTypePtr conn, char *buff, int len);
void httpdPlatDisconnect(ConnTypePtr conn);
void httpdPlatDisableTimeout(ConnTypePtr conn);
void httpdPlatInit(int port, int maxConnCt);
void httpdPlatLock();
void httpdPlatUnlock();

#endif