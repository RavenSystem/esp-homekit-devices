#ifndef HTTP_OTA_H
#define HTTP_OTA_H

/**
 * @file ota_update.h
 * @author Andrea Greco <a.greco@4sigma.it>
 * @brief File containing struct and function ota update
 * File containing struct and function ota update.
 * Ota use remote HTTP server in internet, for dowload Firmaware and sha256 file.
 * Firmaware is compiled file stripped, contained in folder firmaware.
 * Sha256 is a file that contains a sha256, of Firmaware.
 * If enabled 256 is checked during firmaware download.
 */
#include "http_buffered_client.h"

typedef enum {
    // Keep this aligned with \ref HTTP_Client_State
    OTA_DNS_LOOKUP_FALLIED        = HTTP_DNS_LOOKUP_FALLIED,/**< DNS lookup has fallied */
    OTA_SOCKET_ALLOCATION_FALLIED = HTTP_SOCKET_ALLOCATION_FALLIED,/**< Impossible allocate required socket */
    OTA_SOCKET_CONNECTION_FALLIED = HTTP_SOCKET_CONNECTION_FALLIED,/**< Server unreachable, impossible connect */
    OTA_SHA_DONT_MATCH            = HTTP_SHA_DONT_MATCH,/** Sha256 sum does not fit downloaded sha256 */
    OTA_REQUEST_SEND_FALLIED      = HTTP_REQUEST_SEND_FALLIED,/**< Impossible send HTTP request */
    OTA_DOWLOAD_SIZE_NOT_MATCH    = HTTP_DOWLOAD_SIZE_NOT_MATCH, /**< Dowload size don't match with server declared size */

    // Ota error
    OTA_ONE_SLOT_ONLY             = 20,/**< rboot has only one slot configured, impossible switch it */
    OTA_FAIL_SET_NEW_SLOT         = 21,/**< rboot cannot switch between rom */
    OTA_IMAGE_VERIFY_FALLIED      = 22,/**< Dowloaded image binary checsum is fallied */
    OTA_UPDATE_DONE               = 23, /**< Ota has completed upgrade process, all ready for system software reset */

    OTA_HTTP_OK                   = 200,/** HTTP server has response 200, Ok */
    OTA_HTTP_NOTFOUND             = 404,/** HTTP server has response 404, file not found */
} OTA_err;


/**
 * \brief Create ota info.
 * Struct that contains all info for start ota.
 */
typedef struct {
    const char *server;      /**< Server domain */
    const char *port;        /**< Server port   */
    const char *binary_path; /**< Server Path dowload new update binary */
    const char *sha256_path; /**< Server Path of SHA256 sum for check binary, could be NULL, check will be skipped */
} ota_info;

/**
 * \brief Start ota update.
 * Start Ota update.
 * Ota_info contains all information about file to download, port, server.
 * \param ota_info_par ptr to ota info.
 * In case of success, and ota update is right done, \ref finish_cb is called before ESP8266 reset.
 * \return http server return Code, check out \ref HTTP_Client_State for all code.
 */
OTA_err ota_update(ota_info *ota_info_par);
#endif // ifndef HTTP_OTA_H
