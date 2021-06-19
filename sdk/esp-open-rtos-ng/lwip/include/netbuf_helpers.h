/* Some netbuf helpers that should probably be rolled into a patch to lwip soon */
#ifndef _NETBUF_HELPERS_H
#define _NETBUF_HELPERS_H
#include "lwip/netbuf.h"

/* Read a 16 bit wide unsigned integer, stored host order, from the netbuf */
inline static u16_t netbuf_read_u16_h(struct netbuf *netbuf, u16_t offs)
{
    u16_t raw;
    netbuf_copy_partial(netbuf, &raw, 2, offs);
    return raw;
}

/* Read a 16 bit wide unsigned integer, stored network order, from the netbuf */
inline static u16_t netbuf_read_u16_n(struct netbuf *netbuf, u16_t offs)
{
    return ntohs(netbuf_read_u16_h(netbuf, offs));
}

/* Read an 8 bit unsigned integer from the netbuf */
inline static u8_t netbuf_read_u8(struct netbuf *netbuf, u16_t offs)
{
    u8_t result;
    netbuf_copy_partial(netbuf, &result, 1, offs);
    return result;
}

#endif
