/*
 * Simple test using LWIP SNTP
 */

/*-
 * Copyright (c) 2018, Jeff Kletsky
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *     * Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <esp8266.h>
#include <esp/uart.h>

#include <FreeRTOS.h>
#include <task.h>

/* espressif/esp_system.h uses "bool" but fails to #include */
#include <stdbool.h>
#include <espressif/esp_common.h>
#include <espressif/esp_wifi.h>

/*  For sdk_system_restart_in_nmi() */
#include <esplibs/libmain.h>

#include <stdio.h>
#include <stdlib.h>

#include <lwip/apps/sntp.h>


#define BLUE_LED 2
#define LED_ON 0
#define LED_OFF 1

#define WAIT_FOR_UART_SECS 5  /* 2 might be OK */

#define TV2LD(TV) ((long double)TV.tv_sec + (long double)TV.tv_usec * 1.e-6)

void
sntp_task(void *pvParameters) {

    while (sdk_wifi_station_get_connect_status() != STATION_GOT_IP) {
        vTaskDelay(10);
    };

    /*
     * Set one of SNTP_OPMODE_LISTENONLY or SNTP_OPMODE_POLL
     */


    /*
     * SNTP_OPMODE_LISTENONLY
     *     just needs the mode set, no server names required
     *     (requires broadcast NTP on your network)
     */

    sntp_setoperatingmode(SNTP_OPMODE_LISTENONLY);

    /*
     * SNTP_OPMODE_POLL
     *     Needs one or more server names set
     *     additional servers are "fail over"
     *     Can use a DNS name or an address literal
     *     LWIP can also be configured with SNTP_GET_SERVERS_FROM_DHCP
     *     (DHCP-specified SNTP servers untested at this time)
     *
     *  NOTE: Early testing with polling shows higher deviations
     *        than seen with broadcast, even with RTT compensation
     *        Cause unknown at this time, but believed to be within SNTP
     *        amd not part of timekeeping itself.
     */

/*
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "ntp_a.example.com");
    sntp_setservername(1, "ntp_b.example.com");
    sntp_setservername(2, "ntp_c.example.com");
*/

    /* Once set up, this is all it takes */
    sntp_init();

    /*
     * Have high-priority thread "parked", might as well use it
     * Show calling gettimeofday() once an hour to check for timer wrap
     * (the SNTP process itself, if connected, should be sufficient)
     */
    while (1) {
        vTaskDelay(60 * 60 * (1000 / portTICK_PERIOD_MS));
        printf("gettimeofday(NULL, NULL)\n");
        gettimeofday(NULL, NULL);
    }
}



void
user_init(void)
{
    uart_set_baud(0, 115200);
    sdk_wifi_set_opmode(STATION_MODE);
    /*
     * Run at a high enough priority so that the initial time-set doesn't get interrupted
     * Later calls and listen-only mode calls run in the high-priority "tcpip_thread" (LWIP)
     *
     * While 196 heap seemed sufficient for many tests, use of an NTP pool
     * with DNS caused heap errors/warnings on starting NTP at 256 heap
     */
    xTaskCreate(sntp_task, "SNTP task", 288, NULL, 6, NULL);
}
