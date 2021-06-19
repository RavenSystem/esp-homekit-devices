#include <lwip/api.h>
#include <string.h>
#include <espressif/esp_common.h>

void httpd_task(void *pvParameters)
{
    struct netconn *client = NULL;
    struct netconn *nc = netconn_new(NETCONN_TCP);
    if (nc == NULL) {
        printf("Failed to allocate socket\n");
        vTaskDelete(NULL);
    }
    netconn_bind(nc, IP_ADDR_ANY, 80);
    netconn_listen(nc);
    while (1) {
        err_t err = netconn_accept(nc, &client);
        if (err == ERR_OK) {
            struct netbuf *nb;
            if ((err = netconn_recv(client, &nb)) == ERR_OK) {
                struct sdk_station_config config;
                sdk_wifi_station_get_config(&config);
                const char * buf =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\
                         <root>\
                            <device>\
                                <deviceType>urn:Belkin:device:controllee:1</deviceType>\
                                <friendlyName>hello</friendlyName>\
                                <manufacturer>Belkin International Inc.</manufacturer>\
                                <modelName>Emulated Socket</modelName>\
                                <modelNumber>3.1415</modelNumber>\
                                <UDN>uuid:Socket-1_0-38323636-4558-4dda-9188-cda0e6cc3dc0</UDN>\
                                <serialNumber>221517K0101769</serialNumber>\
                                <binaryState>0</binaryState>\
                                <serviceList>\
                                    <service>\
                                        <serviceType>urn:Belkin:service:basicevent:1</serviceType>\
                                        <serviceId>urn:Belkin:serviceId:basicevent1</serviceId>\
                                        <controlURL>/upnp/control/basicevent1</controlURL>\
                                        <eventSubURL>/upnp/event/basicevent1</eventSubURL>\
                                        <SCPDURL>/eventservice.xml</SCPDURL>\
                                    </service>\
                                </serviceList>\
                            </device>\
                         </root>";
                netconn_write(client, buf, strlen(buf), NETCONN_COPY);
            }
            netbuf_delete(nb);
        }
        printf("Closing connection\n");
        netconn_close(client);
        netconn_delete(client);
    }
}
