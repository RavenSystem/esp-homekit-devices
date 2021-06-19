/**
  ******************************************************************************
  * @file    MQTTESP8266.h
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
#ifndef _MQTT_ESP8266_H_
#define _MQTT_ESP8266_H_

#include <FreeRTOS.h>
#include <portmacro.h>

typedef struct mqtt_timer mqtt_timer_t;

struct mqtt_timer
{
    TickType_t end_time;
};

typedef struct mqtt_network mqtt_network_t;

struct mqtt_network
{
	int my_socket;
	int (*mqttread) (mqtt_network_t*, unsigned char*, int, int);
	int (*mqttwrite) (mqtt_network_t*, unsigned char*, int, int);
};

char mqtt_timer_expired(mqtt_timer_t*);
void mqtt_timer_countdown_ms(mqtt_timer_t*, unsigned int);
void mqtt_timer_countdown(mqtt_timer_t*, unsigned int);
int mqtt_timer_left_ms(mqtt_timer_t*);
void mqtt_timer_init(mqtt_timer_t*);

int mqtt_esp_read(mqtt_network_t*, unsigned char*, int, int);
int mqtt_esp_write(mqtt_network_t*, unsigned char*, int, int);
void mqtt_esp_disconnect(mqtt_network_t*);

void mqtt_network_new(mqtt_network_t* n);
int mqtt_network_connect(mqtt_network_t* n, const char* host, int port);
int mqtt_network_disconnect(mqtt_network_t* n);

#endif /* _MQTT_ESP8266_H_ */
