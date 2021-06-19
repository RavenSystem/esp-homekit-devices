/* Very basic LWIP & FreeRTOS-based DHCP server
 *
 * Based on RFC2131 http://www.ietf.org/rfc/rfc2131.txt
 * ... although not fully RFC compliant yet.
 *
 * TODO
 * * Allow binding on a single interface only (for mixed AP/client mode), lwip seems to make it hard to
 *   listen for or send broadcasts on a specific interface only.
 *
 * * Probably allocates more memory than it should, it should be possible to reuse netbufs in most cases.
 *
 * Part of esp-open-rtos
 * Copyright (C) 2015 Superhouse Automation Pty Ltd
 * BSD Licensed as described in the file LICENSE
 */
#include <string.h>
#include <strings.h>

#include <FreeRTOS.h>
#include <task.h>
#include <lwip/netif.h>
#include <lwip/api.h>
#include "esplibs/libmain.h"

#if (DHCP_DEBUG == LWIP_DBG_ON)
#define debug(s, ...) printf("%s: " s "\n", "DHCP", ## __VA_ARGS__)
#else
#define debug(s, ...)
#endif

/* Grow the size of the lwip dhcp_msg struct's options field, as LWIP
   defaults to a 68 octet options field for its DHCP client, and most
   full-sized clients send us more than this. */
#define DHCP_OPTIONS_LEN 312

#include <lwip/ip.h>
#include <lwip/prot/dhcp.h>
#include <lwip/prot/iana.h>

_Static_assert(sizeof(struct dhcp_msg) == offsetof(struct dhcp_msg, options) + 312, "dhcp_msg_t should have extended options size");

#include <lwip/netbuf.h>

#include "dhcpserver.h"

typedef struct {
    uint8_t hwaddr[NETIF_MAX_HWADDR_LEN];
    uint8_t active;
    uint32_t expires;
} dhcp_lease_t;

typedef struct {
    struct netconn *nc;
    uint8_t max_leases;
    ip4_addr_t first_client_addr;
    struct netif *server_if;
    dhcp_lease_t *leases; /* length max_leases */
    /* Optional router */
    ip4_addr_t router;
    /* Optional DNS server */
    ip4_addr_t dns;
} server_state_t;

/* Only one DHCP server task can run at once, so we have global state
   for it.
*/
static TaskHandle_t dhcpserver_task_handle = NULL;
static server_state_t *state;

/* Handlers for various kinds of incoming DHCP messages */
static void handle_dhcp_discover(struct dhcp_msg *received);
static void handle_dhcp_request(struct dhcp_msg *dhcpmsg);
static void handle_dhcp_release(struct dhcp_msg *dhcpmsg);

static void send_dhcp_nak(struct dhcp_msg *dhcpmsg);

static void dhcpserver_task(void *pxParameter);

/* Utility functions */
static uint8_t *find_dhcp_option(struct dhcp_msg *msg, uint8_t option_num, uint8_t min_length, uint8_t *length);
static uint8_t *add_dhcp_option_byte(uint8_t *opt, uint8_t type, uint8_t value);
static uint8_t *add_dhcp_option_bytes(uint8_t *opt, uint8_t type, void *value, uint8_t len);
static dhcp_lease_t *find_lease_slot(uint8_t *hwaddr);

/* Copy IP address as dotted decimal to 'dest', must be at least 16 bytes long */
inline static void sprintf_ipaddr(const ip4_addr_t *addr, char *dest)
{
    if (addr == NULL)
        sprintf(dest, "NULL");
    else
        sprintf(dest, "%d.%d.%d.%d", ip4_addr1(addr),
                ip4_addr2(addr), ip4_addr3(addr), ip4_addr4(addr));
}

void dhcpserver_start(const ip4_addr_t *first_client_addr, uint8_t max_leases)
{
    /* Stop any existing running dhcpserver */
    if (dhcpserver_task_handle)
        dhcpserver_stop();

    state = malloc(sizeof(server_state_t));
    memset(state, 0, sizeof(*state));
    state->max_leases = max_leases;
    state->leases = calloc(max_leases, sizeof(dhcp_lease_t));
    bzero(state->leases, max_leases * sizeof(dhcp_lease_t));
    // state->server_if is assigned once the task is running - see comment in dhcpserver_task()
    ip4_addr_copy(state->first_client_addr, *first_client_addr);

    /* Clear options */
    ip4_addr_set_zero(&state->router);
    ip4_addr_set_zero(&state->dns);

    xTaskCreate(dhcpserver_task, "DHCP Server", 448, NULL, 2, &dhcpserver_task_handle);
}

void dhcpserver_stop(void)
{
    if (dhcpserver_task_handle) {
        vTaskDelete(dhcpserver_task_handle);
        dhcpserver_task_handle = NULL;

        if (state->nc)
            netconn_delete(state->nc);
        free(state->leases);
        free(state);
        state = NULL;
    }
}

void dhcpserver_set_router(const ip4_addr_t *router)
{
    ip4_addr_copy(state->router, *router);
}

void dhcpserver_set_dns(const ip4_addr_t *dns)
{
    ip4_addr_copy(state->dns, *dns);
}

static void dhcpserver_task(void *pxParameter)
{
    /* netif_list isn't assigned until after user_init completes, which is why we do it inside the task */
    state->server_if = netif_list; /* TODO: Make this configurable */

    state->nc = netconn_new (NETCONN_UDP);
    if(!state->nc) {
        debug("DHCP Server Error: Failed to allocate socket.");
        return;
    }

    netconn_bind(state->nc, IP4_ADDR_ANY, LWIP_IANA_PORT_DHCP_SERVER);
    netconn_bind_if (state->nc, netif_get_index(state->server_if));

    while(1)
    {
        struct netbuf *netbuf;
        struct dhcp_msg received = { 0 };

        /* Receive a DHCP packet */
        err_t err = netconn_recv(state->nc, &netbuf);
        if(err != ERR_OK) {
            debug("DHCP Server Error: Failed to receive DHCP packet. err=%d", err);
            continue;
        }

        /* expire any leases that have passed */
        uint32_t now = xTaskGetTickCount();
        for (int i = 0; i < state->max_leases; i++) {
            if (state->leases[i].active) {
                uint32_t expires = state->leases[i].expires - now;
                if (expires >= 0x80000000) {
                    state->leases[i].active = 0;
                }
            }
        }

        ip_addr_t received_ip;
        u16_t port;
        netconn_addr(state->nc, &received_ip, &port);

        if (netbuf_len(netbuf) < offsetof(struct dhcp_msg, options)) {
            /* too short to be a valid DHCP client message */
            netbuf_delete(netbuf);
            continue;
        }
        if(netbuf_len(netbuf) >= sizeof(struct dhcp_msg)) {
           debug("DHCP Server Warning: Client sent more options than we know how to parse. len=%d", netbuf_len(netbuf));
        }

        netbuf_copy(netbuf, &received, sizeof(struct dhcp_msg));
        netbuf_delete(netbuf);

        uint8_t *message_type = find_dhcp_option(&received, DHCP_OPTION_MESSAGE_TYPE,
                                                 DHCP_OPTION_MESSAGE_TYPE_LEN, NULL);
        if(!message_type) {
            debug("DHCP Server Error: No message type field found");
            continue;
        }

#if (DHCP_DEBUG == LWIP_DBG_ON)
        debug("State dump. Message type %d", *message_type);
        for(int i = 0; i < state->max_leases; i++) {
            dhcp_lease_t *lease = &state->leases[i];
            debug("lease slot %d expiry %d hwaddr %02x:%02x:%02x:%02x:%02x:%02x", i, lease->expires, lease->hwaddr[0],
                   lease->hwaddr[1], lease->hwaddr[2], lease->hwaddr[3], lease->hwaddr[4],
                   lease->hwaddr[5]);
        }
#endif

        switch(*message_type) {
        case DHCP_DISCOVER:
            handle_dhcp_discover(&received);
            break;
        case DHCP_REQUEST:
            handle_dhcp_request(&received);
            break;
        case DHCP_RELEASE:
            handle_dhcp_release(&received);
            break;
        default:
            debug("DHCP Server Error: Unsupported message type %d", *message_type);
            break;
        }
    }
}

static void handle_dhcp_discover(struct dhcp_msg *dhcpmsg)
{
    if (dhcpmsg->htype != LWIP_IANA_HWTYPE_ETHERNET)
        return;
    if (dhcpmsg->hlen > NETIF_MAX_HWADDR_LEN)
        return;

    dhcp_lease_t *freelease = find_lease_slot(dhcpmsg->chaddr);
    if(!freelease) {
        debug("DHCP Server: All leases taken.");
        return; /* Nothing available, so do nothing */
    }

    /* Reuse the DISCOVER buffer for the OFFER response */
    dhcpmsg->op = DHCP_BOOTREPLY;
    bzero(dhcpmsg->options, DHCP_OPTIONS_LEN);

    dhcpmsg->yiaddr.addr = lwip_htonl(lwip_ntohl(state->first_client_addr.addr) + (freelease - state->leases));

    uint8_t *opt = (uint8_t *)&dhcpmsg->options;
    opt = add_dhcp_option_byte(opt, DHCP_OPTION_MESSAGE_TYPE, DHCP_OFFER);
    opt = add_dhcp_option_bytes(opt, DHCP_OPTION_SERVER_ID, &state->server_if->ip_addr, 4);
    opt = add_dhcp_option_bytes(opt, DHCP_OPTION_SUBNET_MASK, &state->server_if->netmask, 4);
    if (!ip4_addr_isany_val(state->router)) {
        opt = add_dhcp_option_bytes(opt, DHCP_OPTION_ROUTER, &state->router, 4);
    }
    if (!ip4_addr_isany_val(state->dns)) {
        opt = add_dhcp_option_bytes(opt, DHCP_OPTION_DNS_SERVER, &state->dns, 4);
    }

    uint32_t expiry = htonl(DHCPSERVER_LEASE_TIME);
    opt = add_dhcp_option_bytes(opt, DHCP_OPTION_LEASE_TIME, &expiry, 4);

    opt = add_dhcp_option_bytes(opt, DHCP_OPTION_END, NULL, 0);

    struct netbuf *netbuf = netbuf_new();
    netbuf_alloc(netbuf, sizeof(struct dhcp_msg));
    netbuf_take(netbuf, dhcpmsg, sizeof(struct dhcp_msg));
    netconn_sendto(state->nc, netbuf, IP_ADDR_BROADCAST, 68);
    netbuf_delete(netbuf);
}

static void handle_dhcp_request(struct dhcp_msg *dhcpmsg)
{
    static char ipbuf[16];
    if (dhcpmsg->htype != LWIP_IANA_HWTYPE_ETHERNET)
        return;
    if (dhcpmsg->hlen > NETIF_MAX_HWADDR_LEN)
        return;

    ip4_addr_t requested_ip;
    uint8_t *requested_ip_opt = find_dhcp_option(dhcpmsg, DHCP_OPTION_REQUESTED_IP, 4, NULL);
    if (requested_ip_opt) {
        memcpy(&requested_ip.addr, requested_ip_opt, 4);
    } else if (ip4_addr_cmp(&requested_ip, IP4_ADDR_ANY4)) {
        ip4_addr_copy(requested_ip, dhcpmsg->ciaddr);
    } else {
        debug("DHCP Server Error: No requested IP");
        send_dhcp_nak(dhcpmsg);
        return;
    }

    /* Test the first 4 octets match */
    if (ip4_addr1(&requested_ip) != ip4_addr1(&state->first_client_addr)
       || ip4_addr2(&requested_ip) != ip4_addr2(&state->first_client_addr)
       || ip4_addr3(&requested_ip) != ip4_addr3(&state->first_client_addr)) {
        sprintf_ipaddr(&requested_ip, ipbuf);
        debug("DHCP Server Error: %s not an allowed IP", ipbuf);
        send_dhcp_nak(dhcpmsg);
        return;
    }
    /* Test the last octet is in the MAXCLIENTS range */
    int16_t octet_offs = ip4_addr4(&requested_ip) - ip4_addr4(&state->first_client_addr);
    if(octet_offs < 0 || octet_offs >= state->max_leases) {
        debug("DHCP Server Error: Address out of range");
        send_dhcp_nak(dhcpmsg);
        return;
    }

    dhcp_lease_t *requested_lease = state->leases + octet_offs;
    if (requested_lease->active && memcmp(requested_lease->hwaddr, dhcpmsg->chaddr,dhcpmsg->hlen))
    {
        debug("DHCP Server Error: Lease for address already taken");
        send_dhcp_nak(dhcpmsg);
        return;
    }

    memcpy(requested_lease->hwaddr, dhcpmsg->chaddr, dhcpmsg->hlen);
    sprintf_ipaddr(&requested_ip, ipbuf);
    debug("DHCP lease addr %s assigned to MAC %02x:%02x:%02x:%02x:%02x:%02x", ipbuf, requested_lease->hwaddr[0],
           requested_lease->hwaddr[1], requested_lease->hwaddr[2], requested_lease->hwaddr[3], requested_lease->hwaddr[4],
           requested_lease->hwaddr[5]);
    uint32_t now = xTaskGetTickCount();
    requested_lease->expires = now + DHCPSERVER_LEASE_TIME * configTICK_RATE_HZ;
    requested_lease->active = 1;

    sdk_wifi_softap_set_station_info(requested_lease->hwaddr, &requested_ip);

    /* Reuse the REQUEST message as the ACK message */
    dhcpmsg->op = DHCP_BOOTREPLY;
    bzero(dhcpmsg->options, DHCP_OPTIONS_LEN);

    ip4_addr_copy(dhcpmsg->yiaddr, requested_ip);

    uint8_t *opt = (uint8_t *)&dhcpmsg->options;
    opt = add_dhcp_option_byte(opt, DHCP_OPTION_MESSAGE_TYPE, DHCP_ACK);
    uint32_t expiry = htonl(DHCPSERVER_LEASE_TIME);
    opt = add_dhcp_option_bytes(opt, DHCP_OPTION_LEASE_TIME, &expiry, 4);
    opt = add_dhcp_option_bytes(opt, DHCP_OPTION_SERVER_ID, &state->server_if->ip_addr, 4);
    opt = add_dhcp_option_bytes(opt, DHCP_OPTION_SUBNET_MASK, &state->server_if->netmask, 4);
    if (!ip4_addr_isany_val(state->router)) {
        opt = add_dhcp_option_bytes(opt, DHCP_OPTION_ROUTER, &state->router, 4);
    }
    if (!ip4_addr_isany_val(state->dns)) {
        opt = add_dhcp_option_bytes(opt, DHCP_OPTION_DNS_SERVER, &state->dns, 4);
    }

    opt = add_dhcp_option_bytes(opt, DHCP_OPTION_END, NULL, 0);

    struct netbuf *netbuf = netbuf_new();
    netbuf_alloc(netbuf, sizeof(struct dhcp_msg));
    netbuf_take(netbuf, dhcpmsg, sizeof(struct dhcp_msg));
    netconn_sendto(state->nc, netbuf, IP_ADDR_BROADCAST, 68);
    netbuf_delete(netbuf);
}

static void handle_dhcp_release(struct dhcp_msg *dhcpmsg)
{
    dhcp_lease_t *lease = find_lease_slot(dhcpmsg->chaddr);
    if (lease) {
        lease->active = 0;
        lease->expires = 0;
    }
}

static void send_dhcp_nak(struct dhcp_msg *dhcpmsg)
{
    /* Reuse 'dhcpmsg' for the NAK */
    dhcpmsg->op = DHCP_BOOTREPLY;
    bzero(dhcpmsg->options, DHCP_OPTIONS_LEN);

    uint8_t *opt = (uint8_t *)&dhcpmsg->options;
    opt = add_dhcp_option_byte(opt, DHCP_OPTION_MESSAGE_TYPE, DHCP_NAK);
    opt = add_dhcp_option_bytes(opt, DHCP_OPTION_SERVER_ID, &state->server_if->ip_addr, 4);
    opt = add_dhcp_option_bytes(opt, DHCP_OPTION_END, NULL, 0);

    struct netbuf *netbuf = netbuf_new();
    netbuf_alloc(netbuf, sizeof(struct dhcp_msg));
    netbuf_take(netbuf, dhcpmsg, sizeof(struct dhcp_msg));
    netconn_sendto(state->nc, netbuf, IP_ADDR_BROADCAST, 68);
    netbuf_delete(netbuf);
}

static uint8_t *find_dhcp_option(struct dhcp_msg *msg, uint8_t option_num, uint8_t min_length, uint8_t *length)
{
    uint8_t *start = (uint8_t *)&msg->options;
    uint8_t *msg_end = (uint8_t *)msg + sizeof(struct dhcp_msg);

    for (uint8_t *p = start; p < msg_end-2;) {
        uint8_t type = *p++;
        uint8_t len = *p++;
        if (type == DHCP_OPTION_END)
            return NULL;
        if (p+len >= msg_end)
            break; /* We've overrun our valid DHCP message size, or this isn't a valid option */
        if (type == option_num) {
            if (len < min_length)
                break;
            if (length)
                *length = len;
            return p; /* start of actual option data */
        }
        p += len;
    }
    return NULL; /* Not found */
}

static uint8_t *add_dhcp_option_byte(uint8_t *opt, uint8_t type, uint8_t value)
{
    *opt++ = type;
    *opt++ = 1;
    *opt++ = value;
    return opt;
}

static uint8_t *add_dhcp_option_bytes(uint8_t *opt, uint8_t type, void *value, uint8_t len)
{
    *opt++ = type;
    if (len) {
        *opt++ = len;
        memcpy(opt, value, len);
    }
    return opt+len;
}

/* Find a free DHCP lease, or a lease already assigned to 'hwaddr' */
static dhcp_lease_t *find_lease_slot(uint8_t *hwaddr)
{
    dhcp_lease_t *empty_lease = NULL;
    for (int i = 0; i < state->max_leases; i++) {
        if (!state->leases[i].active && !empty_lease)
            empty_lease = &state->leases[i];
        else if (memcmp(hwaddr, state->leases[i].hwaddr, 6) == 0)
            return &state->leases[i];
    }
    return empty_lease;
}
