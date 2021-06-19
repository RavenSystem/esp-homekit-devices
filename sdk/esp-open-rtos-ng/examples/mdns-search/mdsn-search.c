/*
 * Example mDNS service search.
 *
 * Copyright (C) 2019 OurAirQuality.org
 *
 * Licensed under the Apache License, Version 2.0, January 2004 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *      http://www.apache.org/licenses/
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE CONTRIBUTORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS WITH THE SOFTWARE.
 *
 */

#include <string.h>
#include <ctype.h>

#include <espressif/esp_common.h>
#include <espressif/user_interface.h>
#include <esp/uart.h>
#include <FreeRTOS.h>
#include <task.h>

#include "lwip/sockets.h"
#include "lwip/apps/mdns.h"
#include "lwip/prot/dns.h"
#include "lwip/ip6.h"
#include "lwip/ip6_addr.h"

#include "wificfg/wificfg.h"

#if !LWIP_MDNS_SEARCH
#error "The LWIP_MDNS_SEARCH feature must be set."
#endif

#define SEARCH_SERVICE "_http"
#define SEARCH_PROTO DNSSD_PROTO_TCP

static void print_domain(const u8_t *src)
{
  u8_t i;

  while (*src) {
    u8_t label_len = *src;
    src++;
    for (i = 0; i < label_len; i++) {
        putchar(src[i]);
    }
    src += label_len;
    printf(".");
  }
}

void result_fn(struct mdns_answer *answer, const char *varpart,
               int varlen, int flags, void *arg)
{
    size_t i;

    printf("Domain ");
    print_domain(answer->info.domain->name);
    printf(" ");

    switch (answer->info.type) {
    case DNS_RRTYPE_PTR:
        printf("PTR ");
        print_domain((u8_t *)varpart);
        printf("\n");
        break;

    case DNS_RRTYPE_SRV:
        /* Priority, weight, port fields. */
        printf("SRV ");
        print_domain((u8_t *)varpart + 6);
        printf(" port %u\n", ((u16_t)varpart[4] << 8) | varpart[5]);
        break;

    case DNS_RRTYPE_TXT:
        printf("TXT \"%s\"\n", varpart);
        break;

    case DNS_RRTYPE_A:
        if (varlen == sizeof(ip4_addr_t)) {
            ip4_addr_t addr;
            char buf[16];
            char *p;
            SMEMCPY(&addr, varpart, sizeof(ip4_addr_t));
            p = ip4addr_ntoa_r(&addr, buf, sizeof(buf));
            if (p) printf("A %s\n", p);
        }
        break;

#if LWIP_IPV6
    case DNS_RRTYPE_AAAA:
        if (varlen == sizeof(ip6_addr_p_t)) {
            ip6_addr_p_t addrp;
            ip6_addr_t addr;
            char buf[64];
            char *p;
            SMEMCPY(addrp.addr, varpart, sizeof(ip6_addr_p_t));
            ip6_addr_copy_from_packed(addr, addrp);
            p = ip6addr_ntoa_r(&addr, buf, sizeof(buf));
            if (p) printf("AAAA %s\n", p);
        }
        break;
#endif

    default:
        printf("Unexpected type %u class %u, ans %p, part %p, len %d, flags %x, arg %p\n", answer->info.type, answer->info.klass, answer, varpart, varlen, flags, arg);
        for (i = 0; i < varlen; i++)
            printf(" %02x", varpart[i]);
        printf("\n");
    }
}

static void mdns_search_task(void *pvParameters)
{
    err_t err;
    u8_t request_id;
    struct netif *station_netif = sdk_system_get_netif(STATION_IF);

    if (station_netif == NULL) {
        vTaskDelete(NULL);
    }

    for (;;) {
        printf("Starting mDNS search for %s : ", SEARCH_SERVICE);
        LOCK_TCPIP_CORE();
        err = mdns_search_service(NULL, SEARCH_SERVICE,
                                  SEARCH_PROTO,
                                  station_netif,
                                  result_fn, NULL,
                                  &request_id);
        UNLOCK_TCPIP_CORE();
        printf("request_id %d, error %d\n", request_id, err);

        vTaskDelay(10000 / portTICK_PERIOD_MS);
        printf("Stopping mDNS search\n");

        LOCK_TCPIP_CORE();
        mdns_search_stop(request_id);
        UNLOCK_TCPIP_CORE();
    }
}

void user_init(void)
{
    uart_set_baud(0, 115200);
    printf("SDK version:%s\n", sdk_system_get_sdk_version());

    wificfg_init(80, NULL);

    xTaskCreate(&mdns_search_task, "mDNS search", 384, NULL, 1, NULL);
}
