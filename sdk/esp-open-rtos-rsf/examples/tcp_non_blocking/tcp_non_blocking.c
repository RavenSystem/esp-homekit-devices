/*
   The ESP in the example runs a echo server on 172.16.0.1 (port 50 and 100 ) that
   outputs information about your ip/port then echo all text you write.
   It is manage multiple connection and multiple port with one task.

   This example code is in the public domain.
 */
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <espressif/esp_common.h>
#include <esp8266.h>
#include <esp/uart.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <dhcpserver.h>
#include <lwip/api.h>

#define AP_SSID "esp-open-rtos AP"
#define AP_PSK "esp-open-rtos"
#define ECHO_PORT_1 50
#define ECHO_PORT_2 100
#define EVENTS_QUEUE_SIZE 10

#ifdef CALLBACK_DEBUG
#define debug(s, ...) printf("%s: " s "\n", "Cb:", ## __VA_ARGS__)
#else
#define debug(s, ...)
#endif

QueueHandle_t xQueue_events;
typedef struct {
    struct netconn *nc ;
    uint8_t type ;
} netconn_events;

/*
 * This function will be call in Lwip in each event on netconn
 */
static void netCallback(struct netconn *conn, enum netconn_evt evt, uint16_t length)
{
    //Show some callback information (debug)
    debug("sock:%u\tsta:%u\tevt:%u\tlen:%u\ttyp:%u\tfla:%02X\terr:%d", \
            (uint32_t)conn,conn->state,evt,length,conn->type,conn->flags,conn->last_err);

    netconn_events events ;

    //If netconn got error, it is close or deleted, dont do treatments on it.
    if (conn->pending_err)
    {
        return;
    }
    //Treatments only on rcv events.
    switch (evt) {
    case NETCONN_EVT_RCVPLUS:
        events.nc = conn ;
        events.type = evt ;
        break;
    default:
        return;
        break;
    }

    //Send the event to the queue
    xQueueSend(xQueue_events, &events, 1000);

}

/*
 *  Initialize a server netconn and listen port
 */
static void set_tcp_server_netconn(struct netconn **nc, uint16_t port, netconn_callback callback)
{
    if(nc == NULL)
    {
        printf("%s: netconn missing .\n",__FUNCTION__);
        return;
    }
    *nc = netconn_new_with_callback(NETCONN_TCP, netCallback);
    if(!*nc) {
        printf("Status monitor: Failed to allocate netconn.\n");
        return;
    }
    netconn_set_nonblocking(*nc,NETCONN_FLAG_NON_BLOCKING);
    //netconn_set_recvtimeout(*nc, 10);
    netconn_bind(*nc, IP_ADDR_ANY, port);
    netconn_listen(*nc);
}

/*
 *  Close and delete a socket properly
 */
static void close_tcp_netconn(struct netconn *nc)
{
    nc->pending_err=ERR_CLSD; //It is hacky way to be sure than callback will don't do treatment on a netconn closed and deleted
    netconn_close(nc);
    netconn_delete(nc);
}

/*
 *  This task manage each netconn connection without block anything
 */
static void nonBlockingTCP(void *pvParameters)
{

    struct netconn *nc = NULL; // To create servers

    set_tcp_server_netconn(&nc, ECHO_PORT_1, netCallback);
    printf("Server netconn %u ready on port %u.\n",(uint32_t)nc, ECHO_PORT_1);
    set_tcp_server_netconn(&nc, ECHO_PORT_2, netCallback);
    printf("Server netconn %u ready on port %u.\n",(uint32_t)nc, ECHO_PORT_2);

    struct netbuf *netbuf = NULL; // To store incoming Data
    struct netconn *nc_in = NULL; // To accept incoming netconn
    //
    char buf[50];
    char* buffer;
    uint16_t len_buf;

    while(1) {

        netconn_events events;
        xQueueReceive(xQueue_events, &events, portMAX_DELAY); // Wait here an event on netconn

        if (events.nc->state == NETCONN_LISTEN) // If netconn is a server and receive incoming event on it
        {
            printf("Client incoming on server %u.\n", (uint32_t)events.nc);
            int err = netconn_accept(events.nc, &nc_in);
            if (err != ERR_OK)
            {
                if(nc_in)
                    netconn_delete(nc_in);
            }
            printf("New client is %u.\n",(uint32_t)nc_in);
            ip_addr_t client_addr; //Address port
            uint16_t client_port; //Client port
            netconn_peer(nc_in, &client_addr, &client_port);
            snprintf(buf, sizeof(buf), "Your address is %d.%d.%d.%d:%u.\r\n",
                    ip4_addr1(&client_addr), ip4_addr2(&client_addr),
                    ip4_addr3(&client_addr), ip4_addr4(&client_addr),
                    client_port);
            netconn_write(nc_in, buf, strlen(buf), NETCONN_COPY);
        }
        else if(events.nc->state != NETCONN_LISTEN) // If netconn is the client and receive data
        {
            if ((netconn_recv(events.nc, &netbuf)) == ERR_OK) // data incoming ?
            {
                do
                {
                    netbuf_data(netbuf, (void*)&buffer, &len_buf);
                    netconn_write(events.nc, buffer, strlen(buffer), NETCONN_COPY);
                    printf("Client %u send: %s\n",(uint32_t)events.nc,buffer);
                }
                while (netbuf_next(netbuf) >= 0);
                netbuf_delete(netbuf);
            }
            else
            {
                close_tcp_netconn(events.nc);
                printf("Error read netconn %u, close it \n",(uint32_t)events.nc);
            }
        }
    }
}

void user_init(void)
{
    uart_set_baud(0, 115200);
    sdk_os_delay_us(500); // Wait UART
    printf("SDK version:%s\n", sdk_system_get_sdk_version());

    sdk_wifi_set_opmode(SOFTAP_MODE);
    struct ip_info ap_ip;
    IP4_ADDR(&ap_ip.ip, 172, 16, 0, 1);
    IP4_ADDR(&ap_ip.gw, 0, 0, 0, 0);
    IP4_ADDR(&ap_ip.netmask, 255, 255, 0, 0);
    sdk_wifi_set_ip_info(1, &ap_ip);

    struct sdk_softap_config ap_config = {
            .ssid = AP_SSID,
            .ssid_hidden = 0,
            .channel = 3,
            .ssid_len = strlen(AP_SSID),
            .authmode = AUTH_WPA_WPA2_PSK,
            .password = AP_PSK,
            .max_connection = 3,
            .beacon_interval = 100,
    };
    sdk_wifi_softap_set_config(&ap_config);

    ip_addr_t first_client_ip;
    IP4_ADDR(&first_client_ip, 172, 16, 0, 2);
    dhcpserver_start(&first_client_ip, 4);
    printf("DHCP started\n");

    //Create a queue to store events on netconns
    xQueue_events = xQueueCreate( EVENTS_QUEUE_SIZE, sizeof(netconn_events));

    xTaskCreate(nonBlockingTCP, "lwiptest_noblock", 512, NULL, 2, NULL);
}
