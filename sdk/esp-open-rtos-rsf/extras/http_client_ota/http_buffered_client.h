#ifndef HTTP_BUFFERED_CLIENT
#define HTTP_BUFFERED_CLIENT

typedef unsigned int (*http_final_cb)(char *buff, uint16_t size);

typedef enum  {
    HTTP_DNS_LOOKUP_FALLIED        = 1,
    HTTP_SOCKET_ALLOCATION_FALLIED = 2,
    HTTP_SOCKET_CONNECTION_FALLIED = 3,
    HTTP_SHA_DONT_MATCH            = 4,
    HTTP_REQUEST_SEND_FALLIED      = 5,
    HTTP_DOWLOAD_SIZE_NOT_MATCH    = 6,
    HTTP_OK                        = 200,
    HTTP_NOTFOUND                  = 404,
} HTTP_Client_State;

typedef struct  {
    const char *  server;
    const char *  port;
    const char *  path;
    char *        buffer;
    uint16_t      buffer_size;
    http_final_cb buffer_full_cb;
    http_final_cb final_cb;
} Http_client_info;

HTTP_Client_State HttpClient_dowload(Http_client_info *info);

#endif // ifndef HTTP_BUFFERED_CLIENT
