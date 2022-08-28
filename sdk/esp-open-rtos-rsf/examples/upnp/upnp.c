#include <string.h>
#include <lwip/udp.h>
#include <lwip/igmp.h>
#include <lwip/ip_addr.h>
#include <espressif/esp_common.h>
#include "upnp.h"

#define UPNP_MCAST_GRP  ("239.255.255.250")
#define UPNP_MCAST_PORT (1900)

static const char* get_my_ip(void)
{
    static char ip[16] = "0.0.0.0";
    ip[0] = 0;
    struct ip_info ipinfo;
    (void) sdk_wifi_get_ip_info(STATION_IF, &ipinfo);
    snprintf(ip, sizeof(ip), IPSTR, IP2STR(&ipinfo.ip));
    return (char*) ip;
}

/**
  * @brief This function joins a multicast group with the specified ip/port
  * @param group_ip the specified multicast group ip
  * @param group_port the specified multicast port number
  * @param recv the lwip UDP callback
  * @retval udp_pcb* or NULL if joining failed
  */
static struct udp_pcb* mcast_join_group(const char *group_ip, uint16_t group_port, void (* recv)(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, u16_t port))
{
    bool status = false;
    struct udp_pcb *upcb;

    printf("Joining mcast group %s:%d\n", group_ip, group_port);
    do {
        upcb = udp_new();
        if (!upcb) {
            printf("Error, udp_new failed");
            break;
        }
        udp_bind(upcb, IP4_ADDR_ANY, group_port);
        struct netif* netif = sdk_system_get_netif(STATION_IF);
        if (!netif) {
            printf("Error, netif is null");
            break;
        }
        if (!(netif->flags & NETIF_FLAG_IGMP)) {
            netif->flags |= NETIF_FLAG_IGMP;
            igmp_start(netif);
        }
        ip4_addr_t ipgroup;
        ip4addr_aton(group_ip, &ipgroup);
        err_t err = igmp_joingroup_netif(netif, &ipgroup);
        if (ERR_OK != err) {
            printf("Failed to join multicast group: %d", err);
            break;
        }
        status = true;
    } while(0);

    if (status) {
        printf("Join successs\n");
        udp_recv(upcb, recv, upcb);
    } else {
        if (upcb) {
            udp_remove(upcb);
        }
        upcb = NULL;
    }
    return upcb;
}

static void send_udp(struct udp_pcb *upcb, const ip_addr_t *addr, u16_t port)
{
    struct pbuf *p;
    char msg[500];
    snprintf(msg, sizeof(msg),
        "HTTP/1.1 200 OK\r\n"
        "CACHE-CONTROL: max-age=86400\r\n"
        "DATE: Fri, 15 Apr 2016 04:56:29 GMT\r\n"
        "EXT:\r\n"
        "LOCATION: http://%s:80/setup.xml\r\n"
        "OPT: \"http://schemas.upnp.org/upnp/1/0/\"; ns=01\r\n"
        "01-NLS: b9200ebb-736d-4b93-bf03-835149d13983\r\n"
        "SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
        "ST: urn:Belkin:device:**\r\n"
        "USN: uuid:Socket-1_0-38323636-4558-4dda-9188-cda0e6cc3dc0::urn:Belkin:device:**\r\n"
        "X-User-Agent: redsonic\r\n\r\n", get_my_ip());

    p = pbuf_alloc(PBUF_TRANSPORT, strlen(msg)+1, PBUF_RAM);

    if (!p) {
        printf("Failed to allocate transport buffer\n");
    } else {
        memcpy(p->payload, msg, strlen(msg)+1);    
        err_t err = udp_sendto(upcb, p, addr, port);
        if (err < 0) {
            printf("Error sending message: %s (%d)\n", lwip_strerr(err), err);
        } else {
            printf("Sent message '%s'\n", msg);
        }    
        pbuf_free(p);
    }
}

/**
  * @brief This function is called when an UDP datagrm has been received on the port UDP_PORT.
  * @param arg user supplied argument (udp_pcb.recv_arg)
  * @param pcb the udp_pcb which received data
  * @param p the packet buffer that was received
  * @param addr the remote IP address from which the packet was received
  * @param port the remote port from which the packet was received
  * @retval None
  */
static void receive_callback(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    if (p) {
        printf("Msg received port:%d len:%d\n", port, p->len);
        uint8_t *buf = (uint8_t*) p->payload;
        printf("Msg received port:%d len:%d\nbuf: %s\n", port, p->len, buf);
        
        send_udp(upcb, addr, port);

        pbuf_free(p);
    }
}

/**
  * @brief Initialize the upnp server
  * @retval true if init was succcessful
  */
bool upnp_server_init(void)
{
    struct udp_pcb *upcb = mcast_join_group(UPNP_MCAST_GRP, UPNP_MCAST_PORT, receive_callback);
    return (upcb != NULL);
}
