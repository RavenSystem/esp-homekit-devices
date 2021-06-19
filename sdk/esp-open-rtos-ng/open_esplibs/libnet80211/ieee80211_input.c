/* Recreated Espressif libnet80211 ieee80211_input.o contents.

   Copyright (C) 2015 Espressif Systems. Derived from MIT Licensed SDK libraries.
   BSD Licensed as described in the file LICENSE
*/

#include "esplibs/libpp.h"

void IRAM sdk_ieee80211_deliver_data(struct sdk_g_ic_netif_info *netif_info, struct esf_buf *esf_buf) {
    struct netif *netif = netif_info->netif;

    if (netif->flags & NETIF_FLAG_LINK_UP) {
        uint16_t length = esf_buf->length;
        struct pbuf *pbuf = pbuf_alloc_reference(esf_buf->pbuf2->payload, length, PBUF_ALLOC_FLAG_RX | PBUF_TYPE_ALLOC_SRC_MASK_ESP_RX);
        esf_buf->pbuf1 = pbuf;
        pbuf->esf_buf = (void *)esf_buf;
        ethernetif_input(netif, pbuf);
        return;
    }

    if (esf_buf)
        sdk_ppRecycleRxPkt(esf_buf);

    return;
}
