/*
 * Binary Ping
 *
 * Copyright 2020 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

/*
 * Based on ping helper example by @maxgerhardt.
 * https://github.com/maxgerhardt/esp-open-rtos-ping-example
 */

#include <string.h>
#include "lwip/opt.h"
#include "lwip/mem.h"
#include "lwip/raw.h"
#include "lwip/icmp.h"
#include "lwip/netif.h"
#include "lwip/sys.h"
#include "lwip/timeouts.h"
#include "lwip/inet_chksum.h"
#include "lwip/prot/ip4.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "ping.h"

/** ping receive timeout - in milliseconds */
#ifndef PING_RCV_TIMEO
#define PING_RCV_TIMEO 500
#endif

/** ping identifier - must fit on a u16_t */
#ifndef PING_ID
#define PING_ID        0xFAA0
#endif

/** ping additional data size to include in the packet */
#ifndef PING_DATA_SIZE
#define PING_DATA_SIZE 2
#endif

/* ping variables */
static u16_t ping_seq_num;

/** Prepare a echo ICMP request */
static void ping_prepare_echo(struct icmp_echo_hdr *iecho, u16_t len) {
    size_t i;
    size_t data_len = len - sizeof(struct icmp_echo_hdr);

    ICMPH_TYPE_SET(iecho, ICMP_ECHO);
    ICMPH_CODE_SET(iecho, 0);
    iecho->chksum = 0;
    iecho->id = PING_ID;
    iecho->seqno = lwip_htons(++ping_seq_num);

    /* fill the additional data buffer with some data */
    for (i = 0; i < data_len; i++) {
        ((char*) iecho)[sizeof(struct icmp_echo_hdr) + i] = (char) i;
    }

    iecho->chksum = inet_chksum(iecho, len);
}

/* Ping using the socket ip */
static err_t ping_send(int s, const ip_addr_t *addr) {
    int err;
    struct icmp_echo_hdr *iecho;
    struct sockaddr_storage to;
    size_t ping_size = sizeof(struct icmp_echo_hdr) + PING_DATA_SIZE;
    LWIP_ASSERT("ping_size is too big", ping_size <= 0xffff);
#if LWIP_IPV6
	if(IP_IS_V6(addr) && !ip6_addr_isipv4mappedipv6(ip_2_ip6(addr))) {
		/* todo: support ICMP6 echo */
		return ERR_VAL;
	}
#endif /* LWIP_IPV6 */

    iecho = (struct icmp_echo_hdr*) mem_malloc((mem_size_t) ping_size);
    if (!iecho) {
        return ERR_MEM;
    }

    ping_prepare_echo(iecho, (u16_t) ping_size);

#if LWIP_IPV4
    if (IP_IS_V4(addr)) {
        struct sockaddr_in *to4 = (struct sockaddr_in*) &to;
        to4->sin_len = sizeof(to4);
        to4->sin_family = AF_INET;
        inet_addr_from_ip4addr(&to4->sin_addr, ip_2_ip4(addr));
    }
#endif /* LWIP_IPV4 */

#if LWIP_IPV6
	if(IP_IS_V6(addr)) {
		struct sockaddr_in6 *to6 = (struct sockaddr_in6*)&to;
		to6->sin6_len    = sizeof(to6);
		to6->sin6_family = AF_INET6;
		inet6_addr_from_ip6addr(&to6->sin6_addr, ip_2_ip6(addr));
	}
#endif /* LWIP_IPV6 */

    err = lwip_sendto(s, iecho, ping_size, 0, (struct sockaddr*) &to,
            sizeof(to));

    mem_free(iecho);

    return (err ? ERR_OK : ERR_VAL);
}

static bool ping_recv(int s) {
    char buf[64];
    int len;
    struct sockaddr_storage from;
    int fromlen = sizeof(from);

    while ((len = lwip_recvfrom(s, buf, sizeof(buf), 0,
            (struct sockaddr*) &from, (socklen_t*) &fromlen)) > 0) {
        if (len >= (int) (sizeof(struct ip_hdr) + sizeof(struct icmp_echo_hdr))) {
            ip_addr_t fromaddr;
            memset(&fromaddr, 0, sizeof(fromaddr));

#if LWIP_IPV4
            if (from.ss_family == AF_INET) {
                struct sockaddr_in *from4 = (struct sockaddr_in*) &from;
                inet_addr_to_ip4addr(ip_2_ip4(&fromaddr), &from4->sin_addr);IP_SET_TYPE_VAL(fromaddr, IPADDR_TYPE_V4);
            }
#endif /* LWIP_IPV4 */

#if LWIP_IPV6
			if(from.ss_family == AF_INET6) {
				struct sockaddr_in6 *from6 = (struct sockaddr_in6*)&from;
				inet6_addr_to_ip6addr(ip_2_ip6(&fromaddr), &from6->sin6_addr);
				IP_SET_TYPE_VAL(fromaddr, IPADDR_TYPE_V6);
			}
#endif /* LWIP_IPV6 */

#if LWIP_IPV4
            if (IP_IS_V4_VAL(fromaddr)) {
                struct ip_hdr *iphdr;
                struct icmp_echo_hdr *iecho;

                iphdr = (struct ip_hdr*) buf;
                iecho = (struct icmp_echo_hdr*) (buf + (IPH_HL(iphdr) * 4));
                if (iecho->id == PING_ID &&
                    iecho->seqno == lwip_htons(ping_seq_num) &&
                    ICMPH_TYPE(iecho) == ICMP_ER) {
                    return true;
                }
            }
#endif /* LWIP_IPV4 */
        }
        fromlen = sizeof(from);
    }

    return false;
}

bool ping(ip_addr_t ping_target) {
    int s;
    int ret;
    err_t err;

#if LWIP_SO_SNDRCVTIMEO_NONSTANDARD
	int timeout = PING_RCV_TIMEO;
#else
    struct timeval timeout;
    timeout.tv_sec = PING_RCV_TIMEO / 1000;
    timeout.tv_usec = (PING_RCV_TIMEO % 1000) * 1000;
#endif

#if LWIP_IPV6
	if(IP_IS_V4(ping_target) || ip6_addr_isipv4mappedipv6(ip_2_ip6(ping_target))) {
		s = lwip_socket(AF_INET6, SOCK_RAW, IP_PROTO_ICMP);
	} else {
		s = lwip_socket(AF_INET6, SOCK_RAW, IP6_NEXTH_ICMP6);
	}
#else
    fflush(stdout);
    s = lwip_socket(AF_INET, SOCK_RAW, IP_PROTO_ICMP);

    fflush(stdout);

#endif
    if (s < 0) {
        return false;
    }

    ret = lwip_setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    LWIP_ASSERT("setting receive timeout failed", ret == 0);
    LWIP_UNUSED_ARG(ret);

    bool result = false;
    if ((err = ping_send(s, &ping_target)) == ERR_OK) {
        result = ping_recv(s);
    }
    lwip_close(s);
    
    return result;
}
