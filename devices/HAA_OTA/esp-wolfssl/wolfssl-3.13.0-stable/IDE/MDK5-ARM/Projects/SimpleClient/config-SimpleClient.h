/* config-RTX-TCP-FS.h
 *
 * Copyright (C) 2006-2017 wolfSSL Inc.
 *
 * This file is part of wolfSSL.
 *
 * wolfSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * wolfSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */

// <<< Use Configuration Wizard in Context Menu >>>
// <h> Build Target: Simple Client
//   <s.15>Callee IP Address
//   <i> Default: "192.168.1.100"
#define WOLFSSL_CALLEE_IP           "192.168.10.8"
//   <s.15>Callee Port Number
//   <i> Default: "443"
#define WOLFSSL_CALLEE_PORT           "11111"
//        <o>HTTP GET Option <0=> HTTP Get <1=> SSL/TLS Message
#define MDK_CONF_HTTP_GET 0
#if MDK_CONF_HTTP_GET == 0
    #define WOLFSSL_HTTP_GET "-g"
		#define WOLFSSL_HTTP_GET_COUNT  1
#elif MDK_CONF_HTTP_GET == 1
    #define WOLFSSL_HTTP_GET ""
		#define WOLFSSL_HTTP_GET_COUNT  0
#endif
//        <o>SSL/TLS Version <0=> SSL3 <1=> TLS 1.0 <2=> TLS 1.1 <3=> TLS 1.2
#define MDK_CONF_SSL_VERSION 3
#if MDK_CONF_SSL_VERSION  == 0
    #define WOLFSSL_SSL_VER  "0"
#elif MDK_CONF_SSL_VERSION  == 1
    #define WOLFSSL_SSL_VER  "1"
#elif MDK_CONF_SSL_VERSION  == 2
    #define WOLFSSL_SSL_VER  "2"
#elif MDK_CONF_SSL_VERSION  == 3
    #define WOLFSSL_SSL_VER  "3"
#endif

//     </h>
// <<< end of configuration section >>>
