/**
 * This test verifies basic WiFi communication.
 *
 * Device A creates a WiFi access point and listens on port 23 for incomming
 * connection. When incomming connection occurs it sends a string and waits
 * for the response.
 *
 * Device B connects to a WiFi access point and opens TCP connection to
 * device A on port 23. Then it receives a string and sends it back.
 */
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>

#include <espressif/esp_common.h>
#include "sdk_internal.h"

#include <lwip/api.h>
#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>
#include <lwip/dns.h>
#include <dhcpserver.h>

#include "testcase.h"

#define AP_SSID         "esp-open-rtos-ap"
#define AP_PSK          "esp-open-rtos"
#define SERVER          "172.16.0.1"
#define PORT            23
#define BUF_SIZE        128

DEFINE_TESTCASE(04_wifi_basic, DUAL)

/*********************************************************
 *   WiFi AP part
 *********************************************************/

static void server_task(void *pvParameters)
{

    ip_addr_t first_client_ip;
    IP4_ADDR(&first_client_ip, 172, 16, 0, 2);
    dhcpserver_start(&first_client_ip, 4);

    char buf[BUF_SIZE];
    struct netconn *nc = netconn_new(NETCONN_TCP);
    TEST_ASSERT_TRUE_MESSAGE(nc != 0, "Failed to allocate socket");

    netconn_bind(nc, IP_ADDR_ANY, PORT);
    netconn_listen(nc);

    struct netconn *client = NULL;
    err_t err = netconn_accept(nc, &client);
    TEST_ASSERT_TRUE_MESSAGE(err == ERR_OK, "Error accepting connection");

    ip_addr_t client_addr;
    uint16_t port_ignore;
    netconn_peer(client, &client_addr, &port_ignore);

    snprintf(buf, sizeof(buf), "test ping\r\n");
    printf("Device A: send data: %s\n", buf);
    netconn_write(client, buf, strlen(buf), NETCONN_COPY);

    struct pbuf *pb;
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL_INT_MESSAGE(ERR_OK, netconn_recv_tcp_pbuf(client, &pb),
                "Failed to receive data");
        if (pb->len > 0) {
            memcpy(buf, pb->payload, pb->len);
            buf[pb->len] = 0;
            break;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    TEST_ASSERT_TRUE_MESSAGE(pb->len > 0, "No data received");
    printf("Device A: received data: %s\n", buf);
    TEST_ASSERT_FALSE_MESSAGE(strcmp((const char*)buf, "test pong\r\n"),
            "Received wrong data");

    netconn_delete(client);

    TEST_PASS();
}

static void a_04_wifi_basic(void)
{
    printf("Device A started\n");
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

    xTaskCreate(server_task, "setver_task", 1024, NULL, 2, NULL);
}


/*********************************************************
 *   WiFi client part
 *********************************************************/

static void connect_task(void *pvParameters)
{
    struct sockaddr_in serv_addr;
    char buf[BUF_SIZE];

    // wait for wifi connection
    while (sdk_wifi_station_get_connect_status() != STATION_GOT_IP) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        printf("Waiting for connection to AP\n");
    }

    int s = socket(AF_INET, SOCK_STREAM, 0);
    TEST_ASSERT_TRUE_MESSAGE(s >= 0, "Failed to allocate a socket");

    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_family = AF_INET;

    TEST_ASSERT_TRUE_MESSAGE(inet_aton(SERVER, &serv_addr.sin_addr.s_addr),
            "Failed to set IP address");

    TEST_ASSERT_TRUE_MESSAGE(
            connect(s, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == 0,
            "Socket connection failed");

    bzero(buf, BUF_SIZE);

    int r = 0;
    for (int i = 0; i < 10; i++) {
        r = read(s, buf, BUF_SIZE);
        if (r > 0) {
            break;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    TEST_ASSERT_TRUE_MESSAGE(r > 0, "No data received");

    printf("Device B: received data: %s\n", buf);
    TEST_ASSERT_FALSE_MESSAGE(strcmp((const char*)buf, "test ping\r\n"),
            "Received wrong data");

    snprintf(buf, sizeof(buf), "test pong\r\n");
    printf("Device B: send data: %s\n", buf);
    TEST_ASSERT_EQUAL_INT_MESSAGE(strlen(buf), write(s, buf, strlen(buf)),
            "Error socket writing");

    close(s);

    TEST_PASS();
}

static void b_04_wifi_basic(void)
{
    printf("Device B started\n");
    struct sdk_station_config config = {
        .ssid = AP_SSID,
        .password = AP_PSK,
    };

    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&config);

    xTaskCreate(&connect_task, "connect_task", 1024, NULL, 2, NULL);
}
