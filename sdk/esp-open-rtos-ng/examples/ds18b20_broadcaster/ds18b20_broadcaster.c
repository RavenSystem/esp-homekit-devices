/*
 * This is example of udp broadcaster.
 * It grab data from ds18b20 sensor and emit result over wifi.
 *
 * Author Grzegorz Hetman : ghetman@gmail.com
 * License public domain/CC0
 */

#include <string.h>

#include "espressif/esp_common.h"
#include "esp/uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "lwip/api.h"
#include "ssid_config.h"

// DS18B20 driver
#include "ds18b20/ds18b20.h"

void broadcast_temperature(void *pvParameters)
{

    uint8_t amount = 0;
    uint8_t sensors = 1;
    ds18b20_addr_t addrs[sensors];
    float results[sensors];
    
    // Use GPIO 13 as one wire pin. 
    uint8_t GPIO_FOR_ONE_WIRE = 13;

    char msg[100];

    // Broadcaster part
    err_t err;

    while(1) {

        // Send out some UDP data
        struct netconn* conn;

        // Create UDP connection
        conn = netconn_new(NETCONN_UDP);

        // Connect to local port
        err = netconn_bind(conn, IP_ADDR_ANY, 8004);

        if (err != ERR_OK) {
            netconn_delete(conn);
            printf("%s : Could not bind! (%s)\n", __FUNCTION__, lwip_strerr(err));
            continue;
        }

        err = netconn_connect(conn, IP_ADDR_BROADCAST, 8005);

        if (err != ERR_OK) {
            netconn_delete(conn);
            printf("%s : Could not connect! (%s)\n", __FUNCTION__, lwip_strerr(err));
            continue;
        }

        for(;;) {
            // Search all DS18B20, return its amount and feed 't' structure with result data.
            amount = ds18b20_scan_devices(GPIO_FOR_ONE_WIRE, addrs, sensors);

            if (amount < sensors){
                printf("Something is wrong, I expect to see %d sensors \nbut just %d was detected!\n", sensors, amount);
            }

            ds18b20_measure_and_read_multi(GPIO_FOR_ONE_WIRE, addrs, sensors, results);
            for (int i = 0; i < sensors; ++i)
            {
                // ("\xC2\xB0" is the degree character (U+00B0) in UTF-8)
                sprintf(msg, "Sensor %08x%08x reports: %f \xC2\xB0""C\n", (uint32_t)(addrs[i] >> 32), (uint32_t)addrs[i], results[i]);
                printf("%s", msg);

                struct netbuf* buf = netbuf_new();
                void* data = netbuf_alloc(buf, strlen(msg));

                memcpy (data, msg, strlen(msg));
                err = netconn_send(conn, buf);

                if (err != ERR_OK) {
                    printf("%s : Could not send data!!! (%s)\n", __FUNCTION__, lwip_strerr(err));
                    continue;
                }
                netbuf_delete(buf); // De-allocate packet buffer
            }
            vTaskDelay(1000/portTICK_PERIOD_MS);
        }

        err = netconn_disconnect(conn);
        printf("%s : Disconnected from IP_ADDR_BROADCAST port 12346 (%s)\n", __FUNCTION__, lwip_strerr(err));

        err = netconn_delete(conn);
        printf("%s : Deleted connection (%s)\n", __FUNCTION__, lwip_strerr(err));

        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}

void user_init(void)
{
    uart_set_baud(0, 115200);

    printf("SDK version:%s\n", sdk_system_get_sdk_version());

    // Set led to indicate wifi status.
    sdk_wifi_status_led_install(2, PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);

    struct sdk_station_config config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASS,
    };

    // Required to call wifi_set_opmode before station_set_config.
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&config);

    xTaskCreate(&broadcast_temperature, "broadcast_temperature", 256, NULL, 2, NULL);
}

