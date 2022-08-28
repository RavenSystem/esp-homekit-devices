/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Allan Stockdill-Mander/Ian Craggs - initial API and implementation and/or initial documentation
 *******************************************************************************/

#ifndef __MQTT_CLIENT_C_
#define __MQTT_CLIENT_C_

#include "MQTTPacket.h"
#include "MQTTESP8266.h"

#define MQTT_MAX_PACKET_ID 65535
#define MQTT_MAX_MESSAGE_HANDLERS 5
#define MQTT_MAX_FAIL_ALLOWED  2

enum mqtt_qos {
	MQTT_QOS0,
	MQTT_QOS1,
	MQTT_QOS2
};

// all failure return codes must be negative
enum mqtt_return_code {
	MQTT_READ_ERROR = -4,
	MQTT_DISCONNECTED = -3,
	MQTT_BUFFER_OVERFLOW = -2,
	MQTT_FAILURE = -1,
	MQTT_SUCCESS = 0
};

typedef struct mqtt_message
{
    enum mqtt_qos qos;
    char retained;
    char dup;
    unsigned short id;
    void *payload;
    size_t payloadlen;
} mqtt_message_t;

typedef struct mqtt_message_data
{
    mqtt_string_t* topic;
    mqtt_message_t* message;
} mqtt_message_data_t;

typedef void (*mqtt_message_handler_t)(mqtt_message_data_t*);

struct mqtt_client
{
    unsigned int next_packetid;
    unsigned int command_timeout_ms;
    size_t buf_size, readbuf_size;
    unsigned char *buf;
    unsigned char *readbuf;
    unsigned int keepAliveInterval;
    char ping_outstanding;
    int fail_count;
    int isconnected;

    struct MessageHandlers
    {
        const char* topicFilter;
        void (*fp) (mqtt_message_data_t*);
    } messageHandlers[MQTT_MAX_MESSAGE_HANDLERS];      // Message handlers are indexed by subscription topic

    void (*defaultMessageHandler) (mqtt_message_data_t*);

    mqtt_network_t* ipstack;
    mqtt_timer_t ping_timer;
};

typedef struct mqtt_client mqtt_client_t;

int mqtt_connect(mqtt_client_t* c, mqtt_packet_connect_data_t* options);
int mqtt_publish(mqtt_client_t* c, const char* topic, mqtt_message_t* message);
int mqtt_subscribe(mqtt_client_t* c, const char* topic, enum mqtt_qos qos, mqtt_message_handler_t handler);
int mqtt_unsubscribe(mqtt_client_t* c, const char* topic);
int mqtt_disconnect(mqtt_client_t* c);
int mqtt_yield(mqtt_client_t* c, int timeout_ms);

void mqtt_client_new(mqtt_client_t*, mqtt_network_t*, unsigned int, unsigned char*, size_t, unsigned char*, size_t);

#define mqtt_client_default {0, 0, 0, 0, NULL, NULL, 0, 0, 0}

#endif
