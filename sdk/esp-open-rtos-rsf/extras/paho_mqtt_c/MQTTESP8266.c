/**
  ******************************************************************************
  * @file    MQTTESP8266.c
  * @author  Baoshi <mail(at)ba0sh1(dot)com>
  * @version 0.1
  * @date    Sep 9, 2015
  * @brief   Eclipse Paho ported to ESP8266 RTOS
  *
  ******************************************************************************
  * @copyright
  *
  * Copyright (c) 2015, Baoshi Zhu. All rights reserved.
  * Use of this source code is governed by a BSD-style license that can be
  * found in the LICENSE.txt file.
  *
  * THIS SOFTWARE IS PROVIDED 'AS-IS', WITHOUT ANY EXPRESS OR IMPLIED
  * WARRANTY.  IN NO EVENT WILL THE AUTHOR(S) BE HELD LIABLE FOR ANY DAMAGES
  * ARISING FROM THE USE OF THIS SOFTWARE.
  *
  */

#include <espressif/esp_common.h>
#include <unistd.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <lwip/netdb.h>
#include <lwip/sys.h>
#include <string.h>

#include "MQTTESP8266.h"

char  mqtt_timer_expired(mqtt_timer_t* timer)
{
    TickType_t now = xTaskGetTickCount();
    int32_t left = timer->end_time - now;
    return (left <= 0);
}


void  mqtt_timer_countdown_ms(mqtt_timer_t* timer, unsigned int timeout)
{
    TickType_t now = xTaskGetTickCount();
    timer->end_time = now + timeout / portTICK_PERIOD_MS;
}


void  mqtt_timer_countdown(mqtt_timer_t* timer, unsigned int timeout)
{
    mqtt_timer_countdown_ms(timer, timeout * 1000);
}


int  mqtt_timer_left_ms(mqtt_timer_t* timer)
{
    TickType_t now = xTaskGetTickCount();
    int32_t left = timer->end_time - now;
    return (left < 0) ? 0 : left * portTICK_PERIOD_MS;
}


void  mqtt_timer_init(mqtt_timer_t* timer)
{
    timer->end_time = 0;
}



int  mqtt_esp_read(mqtt_network_t* n, unsigned char* buffer, int len, int timeout_ms)
{
    struct timeval tv;
    fd_set fdset;
    int rc = 0;
    int rcvd = 0;
    FD_ZERO(&fdset);
    FD_SET(n->my_socket, &fdset);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    rc = select(n->my_socket + 1, &fdset, 0, 0, &tv);
    if ((rc > 0) && (FD_ISSET(n->my_socket, &fdset)))
    {
        rcvd = recv(n->my_socket, buffer, len, 0);
    }
    else
    {
        // select fail
        return -1;
    }
    return rcvd;
}


int  mqtt_esp_write(mqtt_network_t* n, unsigned char* buffer, int len, int timeout_ms)
{
    struct timeval tv;
    fd_set fdset;
    int rc = 0;

    FD_ZERO(&fdset);
    FD_SET(n->my_socket, &fdset);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    rc = select(n->my_socket + 1, 0, &fdset, 0, &tv);
    if ((rc > 0) && (FD_ISSET(n->my_socket, &fdset)))
    {
        rc = send(n->my_socket, buffer, len, 0);
    }
    else
    {
        // select fail
        return -1;
    }
    return rc;
}



void  mqtt_network_new(mqtt_network_t* n)
{
    n->my_socket = -1;
    n->mqttread = mqtt_esp_read;
    n->mqttwrite = mqtt_esp_write;
}

static int  host2addr(const char *hostname , struct in_addr *in)
{
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_in *h;
    int rv;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    rv = getaddrinfo(hostname, 0 , &hints , &servinfo);
    if (rv != 0)
    {
        return rv;
    }

    // loop through all the results and get the first resolve
    for (p = servinfo; p != 0; p = p->ai_next)
    {
        h = (struct sockaddr_in *)p->ai_addr;
        in->s_addr = h->sin_addr.s_addr;
    }
    freeaddrinfo(servinfo); // all done with this structure
    return 0;
}


int  mqtt_network_connect(mqtt_network_t* n, const char* host, int port)
{
    struct sockaddr_in addr;
    int ret;

    if (host2addr(host, &(addr.sin_addr)) != 0)
    {
        return -1;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    n->my_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if( n->my_socket < 0 )
    {
        // error
        return -1;
    }
    ret = connect(n->my_socket, ( struct sockaddr *)&addr, sizeof(struct sockaddr_in));
    if( ret < 0 )
    {
        // error
        close(n->my_socket);
        return ret;
    }

    return ret;
}


int  mqtt_network_disconnect(mqtt_network_t* n)
{
    close(n->my_socket);
    n->my_socket = -1;
    return 0;
}
