#ifndef _OTA_TFTP_H
#define _OTA_TFTP_H

#include "lwip/err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*tftp_receive_cb)(size_t bytes_received);

/* TFTP Server OTA Support
 *
 * To use, call ota_tftp_init_server() which will start the TFTP server task
 * and keep it running until a valid image is sent.
 *
 * The server expects to see a valid image sent with filename "filename.bin"
 * and will switch "slots" and reboot if a valid image is received.
 *
 * Note that the server will allow you to flash an "OTA" update that doesn't
 * support OTA itself, and possibly brick the esp requiring serial upload.
 *
 * Example client comment:
 * tftp -m octet ESP_IP -c put firmware/myprogram.bin firmware.bin
 *
 * TFTP protocol implemented as per RFC1350:
 * https://tools.ietf.org/html/rfc1350
 *
 * IMPORTANT: TFTP is not a secure protocol.
 * Only allow TFTP OTA updates on trusted networks.
 *
 *
 * For more details, see https://github.com/SuperHouse/esp-open-rtos/wiki/OTA-Update-Configuration
 */

/* Start a FreeRTOS task to wait to receive an OTA update from a TFTP client.
 *
 * listen_port is the UDP port number to receive TFTP connections on (default TFTP port is 69)
 */
void ota_tftp_init_server(int listen_port);

/* Attempt to make a TFTP client connection and download the specified filename.

   'timeout' is in milliseconds, and is timeout for any UDP exchange
   _not_ the entire download.

   Returns 0 on success, LWIP err.h values for errors.

   Does not change the current firmware slot, or reboot.

   receive_cb: called repeatedly after each successful packet that
   has been written to flash and ACKed.  Can pass NULL to omit.
 */
err_t ota_tftp_download(const char *server, int port, const char *filename,
                        int timeout, int ota_slot, tftp_receive_cb receive_cb);

#define TFTP_PORT 69

#ifdef __cplusplus
}
#endif

#endif
