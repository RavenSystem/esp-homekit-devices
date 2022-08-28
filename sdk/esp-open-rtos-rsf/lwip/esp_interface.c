/* LWIP interface to the ESP WLAN MAC layer driver.

   Based on the sample driver in ethernetif.c. Tweaks based on
   examining esp-lwip eagle_if.c and observed behaviour of the ESP
   RTOS liblwip.a.

   libnet80211.a calls into ethernetif_init & ethernetif_input.

 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 *
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Original Author: Simon Goldschmidt
 * Modified by Angus Gratton based on work by @kadamski/Espressif via esp-lwip project.
 */
#include <string.h>
#include "lwip/opt.h"

#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include <lwip/stats.h>
#include <lwip/snmp.h>
#include "lwip/ip.h"
#include "lwip/ethip6.h"
#include "lwip/priv/tcp_priv.h"
#include "netif/etharp.h"
#include "sysparam.h"
#include "netif/ppp/pppoe.h"
#include "FreeRTOS.h"
#include "task.h"

/* declared in libnet80211.a */
int8_t sdk_ieee80211_output_pbuf(struct netif *ifp, struct pbuf* pb);

/* Define those to better describe your network interface. */
#define IFNAME0 'e'
#define IFNAME1 'n'

/**
 * In this function, the hardware should be initialized.
 * Called from ethernetif_init().
 *
 * @param netif the already initialized lwip network interface structure
 *        for this ethernetif
 */
static void
low_level_init(struct netif *netif)
{
    /* set MAC hardware address length */
    netif->hwaddr_len = ETHARP_HWADDR_LEN;

    /* maximum transfer unit */
    netif->mtu = 1500;

    /* device capabilities */
    /* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_IGMP | NETIF_FLAG_MLD6;

    /* Do whatever else is needed to initialize interface. */
}

/**
 * This function should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
 * @return ERR_OK if the packet could be sent
 *         an err_t value if the packet couldn't be sent
 *
 * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
 *       strange results. You might consider waiting for space in the DMA queue
 *       to become available since the stack doesn't retry to send a packet
 *       dropped because of memory failure (except for the TCP timers).
 */
#define SIZEOF_STRUCT_PBUF        LWIP_MEM_ALIGN_SIZE(sizeof(struct pbuf))
static err_t
low_level_output(struct netif *netif, struct pbuf *p)
{
    /*
     * Note the pbuf reference count is generally one here, but not always. For
     * example a buffer that had been queued by etharp_query() would have had
     * its reference count increased to two, and the caller will free it on that
     * return path.
     */

    /* If the pbuf does not have contiguous data, or there is not enough room
     * for the link layer header, or there are multiple pbufs in the chain then
     * clone a pbuf to output. */
    if ((p->type_internal & PBUF_TYPE_FLAG_STRUCT_DATA_CONTIGUOUS) == 0 ||
        (u8_t *)p->payload < (u8_t *)p + SIZEOF_STRUCT_PBUF + PBUF_LINK_ENCAPSULATION_HLEN ||
        p->next) {
        struct pbuf *q = pbuf_clone(PBUF_RAW_TX, PBUF_RAM, p);
        if (q == NULL) {
            return ERR_MEM;
        }
        sdk_ieee80211_output_pbuf(netif, q);
        /* The sdk will pbuf_ref the pbuf before returning and free it later
         * when it has been sent so free the link to it here. */
        pbuf_free(q);
    } else {
        /* The SDK modifies the eth_hdr, well the first 12 bytes of it at least,
         * but otherwise leaves the payload unmodified so it can be reused by
         * the caller. The only paths that appear to reuse the pbuf are in
         * tcp_out for re-transmission of TCP segments, and these paths check
         * that the number of references has returned to one before reusing the
         * pbuf.
         */
        sdk_ieee80211_output_pbuf(netif, p);
    }

    LINK_STATS_INC(link.xmit);
    return ERR_OK;
}


/**
 * Keep account of the number the PP RX pool buffers being used in lwip,
 * to help make decision about the number of OOSEQ buffers to maintain etc.
 */
volatile uint32_t pp_rx_pool_usage;

/* Support for recycling a pbuf from the sdk rx pool, and accounting for the
 * number of these used in lwip. */
void pp_recycle_rx_pbuf(struct pbuf *p)
{
    LWIP_ASSERT("expected esf_buf", p->esf_buf);
    sdk_system_pp_recycle_rx_pkt(p->esf_buf);
    taskENTER_CRITICAL();
    LWIP_ASSERT("pp_rx_pool_usage underflow", pp_rx_pool_usage > 0);
    pp_rx_pool_usage--;
    taskEXIT_CRITICAL();
}

/* Set to true to copy the rx pbufs on input and free them. The pp rx buffer
 * pool is limited so this allows a large number at the expense of memory.
 */

#define COPY_PP_RX_PBUFS 0

#if TCP_QUEUE_OOSEQ

/* Return the number of ooseq bytes that can be retained given the current
 * size 'n'. */
size_t ooseq_bytes_limit(struct tcp_pcb *pcb)
{
#if COPY_PP_RX_PBUFS
    struct tcp_seg *ooseq = pcb->ooseq;
    size_t ooseq_blen = 0;
    for (; ooseq != NULL; ooseq = ooseq->next) {
        struct pbuf *p = ooseq->p;
        ooseq_blen += p->tot_len;
    }

    size_t free = xPortGetFreeHeapSize();
    ssize_t target = ((ssize_t)free - 8000) + ooseq_blen;

    if (target < 0) {
        target = 0;
    }

    return target;
#else
    /* The pool is pre-allocated so there is no need to look at the dynamic
     * memory usage, just consider the number of them below. */
    return 8000;
#endif
}

/* Return the number of ooseq pbufs that can be retained given the current
 * size 'n'. */
size_t ooseq_pbufs_limit(struct tcp_pcb *pcb)
{
    struct tcp_seg *ooseq = pcb->ooseq;
    size_t ooseq_qlen = 0;
    for (; ooseq != NULL; ooseq = ooseq->next) {
        struct pbuf *p = ooseq->p;
        ooseq_qlen += pbuf_clen(p);
    }

#if COPY_PP_RX_PBUFS
    /* More likely memory limited, but set some limit. */
    ssize_t limit = 10;
#else
    /* Set a small limit if using the pp rx pool, to avoid exhausting it. */
    ssize_t limit = 2;
#endif

    size_t usage = pp_rx_pool_usage;
    ssize_t target = limit - ((ssize_t)usage - ooseq_qlen);

    if (target < 0) {
        target = 0;
    }

    return target;
}

#endif /* TCP_QUEUE_OOSEQ */

/**
 * This function should be called when a packet is ready to be read
 * from the interface. It uses the function low_level_input() that
 * should handle the actual reception of bytes from the network
 * interface. Then the type of the received packet is determined and
 * the appropriate input function is called.
 *
 * @param netif the lwip network interface structure for this ethernetif
 */
/* called from ieee80211_deliver_data with new IP frames */
void ethernetif_input(struct netif *netif, struct pbuf *p)
{
    struct eth_hdr *ethhdr;

    if (p == NULL) {
        return;
    }

    if (p->payload == NULL) {
        return;
    }

    if (netif == NULL) {
        return;
    }

    ethhdr = p->payload;

    /* Account for the number of rx pool buffers being used. */
    taskENTER_CRITICAL();
    uint32_t usage = pp_rx_pool_usage + 1;
    pp_rx_pool_usage = usage;
    taskEXIT_CRITICAL();

    switch(htons(ethhdr->type)) {
	/* IP or ARP packet? */
    case ETHTYPE_IP:
    case ETHTYPE_IPV6:
#if 0
        /* Simulate IP packet loss. */
        if ((random() & 0xff) < 0x10) {
            pbuf_free(p);
            return;
        }
#endif

    case ETHTYPE_ARP:
#if PPPOE_SUPPORT
        /* PPPoE packet? */
    case ETHTYPE_PPPOEDISC:
    case ETHTYPE_PPPOE:
#endif /* PPPOE_SUPPORT */
    {
	/* full packet send to tcpip_thread to process */

#if COPY_PP_RX_PBUFS
        /* Optionally copy the rx pool buffer and free it immediately. This
         * helps avoid exhausting the limited rx buffer pool but uses more
         * memory. */
        struct pbuf *q = pbuf_clone(PBUF_RAW, PBUF_RAM, p);
        pbuf_free(p);
        if (q == NULL) {
            return;
        }
        p = q;
#endif

        if (netif->input(p, netif) != ERR_OK) {
	    LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_input: IP input error\n"));
	    pbuf_free(p);
	    p = NULL;
	}
	break;
    }

    default:
	pbuf_free(p);
	p = NULL;
	break;
    }
}

/* Since the pbuf_type definition has changed in lwip v2 and it is used by the
 * sdk when calling pbuf_alloc, the SDK libraries have been modified to rename
 * their references to pbuf_alloc to _pbufalloc allowing the pbuf_type to be
 * rewritten here. Doing this here keeps this hack out of the lwip code, and
 * ensures that this re-writing is only applied to the sdk calls to pbuf_alloc.
 *
 * The only pbuf types used by the SDK are type 0 for PBUF_RAM when writing
 * data, and type 2 for the received data. The receive data path references
 * internal buffer objects that need to be freed with custom code so a custom
 * pbuf allocation type is used for these.
 *
 * The pbuf_layer is now also the header offset, but the sdk calls only call
 * with a value of 3 which was PBUF_RAW and is now translated to a header
 * offset of zero.
 */
struct pbuf *sdk_pbuf_alloc(pbuf_layer layer, u16_t length, pbuf_type type) {
    if (type == 0) {
        LWIP_ASSERT("Unexpected sdk_pbuf_alloc layer", layer == 3 || layer == 4);
        return pbuf_alloc(PBUF_RAW_TX, length, PBUF_RAM);
    } else if (type == 2) {
        LWIP_ASSERT("Unexpected sdk_pbuf_alloc layer", layer == 3);
        return pbuf_alloc_reference(NULL, length, PBUF_ALLOC_FLAG_RX | PBUF_TYPE_ALLOC_SRC_MASK_ESP_RX);
    } else {
        LWIP_ASSERT("Unexpected pbuf_alloc type", 0);
        for (;;);
    }
}

/**
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 * This function should be passed as a parameter to netif_add().
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error
 */
err_t
ethernetif_init(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));

    /* The hwaddr is currently set by sdk_wifi_station_start or
     * sdk_wifi_softap_start. */

#if LWIP_IPV6
    // Where to do this???
    netif_create_ip6_linklocal_address(netif, 1);
    netif->ip6_autoconfig_enabled = 1;
    printf("ip6 link local address %s\n", ip6addr_ntoa(netif_ip6_addr(netif, 0)));
#endif

#if LWIP_NETIF_HOSTNAME
    /* Initialize interface hostname */
    char *hostname = NULL;
    /* Disabled for now as there were reports of crashes here, sysparam issues */
    /* sysparam_get_string("hostname", &hostname); */
    if (hostname && strlen(hostname) == 0) {
        free(hostname);
        hostname = NULL;
    }
    netif->hostname = hostname;
#endif /* LWIP_NETIF_HOSTNAME */

    /*
     * Initialize the snmp variables and counters inside the struct netif.
     * The last argument should be replaced with your link speed, in units
     * of bits per second.
     */
    NETIF_INIT_SNMP(netif, snmp_ifType_ethernet_csmacd, LINK_SPEED_OF_YOUR_NETIF_IN_BPS);

    // don't touch netif->state here, the field is used internally in the ESP SDK layers
    netif->name[0] = IFNAME0;
    netif->name[1] = IFNAME1;
    /* We directly use etharp_output() here to save a function call.
     * You can instead declare your own function an call etharp_output()
     * from it if you have to do some checks before sending (e.g. if link
     * is available...) */
#if LWIP_IPV4
    netif->output = etharp_output;
#endif /* LWIP_IPV4 */
#if LWIP_IPV6
    netif->output_ip6 = ethip6_output;
#endif /* LWIP_IPV6 */
    netif->linkoutput = low_level_output;

    /* initialize the hardware */
    low_level_init(netif);

    return ERR_OK;
}
