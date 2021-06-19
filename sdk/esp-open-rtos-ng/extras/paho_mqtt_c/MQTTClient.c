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
#include <espressif/esp_common.h>
#include <lwip/arch.h>
#include "MQTTClient.h"

static void new_message_data(mqtt_message_data_t* md, mqtt_string_t* aTopicName, mqtt_message_t* aMessgage) {
    md->topic = aTopicName;
    md->message = aMessgage;
}


static int get_next_packet_id(mqtt_client_t *c) {
    return c->next_packetid = (c->next_packetid == MQTT_MAX_PACKET_ID) ? 1 : c->next_packetid + 1;
}


static int send_packet(mqtt_client_t* c, int length, mqtt_timer_t* timer)
{
    int rc = MQTT_FAILURE,
        sent = 0;

    while (sent < length && !mqtt_timer_expired(timer))
    {
        rc = c->ipstack->mqttwrite(c->ipstack, &c->buf[sent], length - sent, mqtt_timer_left_ms(timer));
        if (rc < 0)  // there was an error writing the data
            break;
        sent += rc;
    }
    if (sent == length)
    {
        mqtt_timer_countdown(&(c->ping_timer), c->keepAliveInterval); // record the fact that we have successfully sent the packet
        rc = MQTT_SUCCESS;
    }
    else
        rc = MQTT_FAILURE;
    return rc;
}


static int decode_packet(mqtt_client_t* c, int* value, int timeout)
{
    unsigned char i;
    int multiplier = 1;
    int len = 0;
    const int MAX_NO_OF_REMAINING_LENGTH_BYTES = 4;

    *value = 0;
    do
    {
        int rc = MQTTPACKET_READ_ERROR;

        if (++len > MAX_NO_OF_REMAINING_LENGTH_BYTES)
        {
            rc = MQTTPACKET_READ_ERROR; /* bad data */
            goto exit;
        }
        rc = c->ipstack->mqttread(c->ipstack, &i, 1, timeout);
        if (rc != 1)
        {
		    goto exit;
        }
        *value += (i & 127) * multiplier;
        multiplier *= 128;
    } while ((i & 128) != 0);
exit:
    return len;
}


// Return packet type. If no packet avilable, return FAILURE, or READ_ERROR if timeout
static int read_packet(mqtt_client_t* c, mqtt_timer_t* timer)
{
    int rc = MQTT_FAILURE;
    mqtt_header_t header = {0};
    int len = 0;
    int rem_len = 0;

    /* 1. read the header byte.  This has the packet type in it */
    if (c->ipstack->mqttread(c->ipstack, c->readbuf, 1, mqtt_timer_left_ms(timer)) != 1)
        goto exit;
    len = 1;
    /* 2. read the remaining length.  This is variable in itself */
    len += decode_packet(c, &rem_len, mqtt_timer_left_ms(timer));
    if (len <= 1 || len + rem_len > c->readbuf_size) /* if packet is too big to fit in our readbuf, abort */
    {
        rc = MQTT_READ_ERROR;
        goto exit;
    }
    mqtt_packet_encode(c->readbuf + 1, rem_len); /* put the original remaining length back into the buffer */
    /* 3. read the rest of the buffer using a callback to supply the rest of the data */
    if (rem_len > 0 && (c->ipstack->mqttread(c->ipstack, c->readbuf + len, rem_len, mqtt_timer_left_ms(timer)) != rem_len))
    {
        rc = MQTT_READ_ERROR;
        goto exit;
    }
    header.byte = c->readbuf[0];
    rc = header.bits.type;
exit:
    return rc;
}


// assume topic filter and name is in correct format
// # can only be at end
// + and # can only be next to separator
static char is_topic_matched(char* topicFilter, mqtt_string_t* topicName)
{
    char* curf = topicFilter;
    char* curn = topicName->lenstring.data;
    char* curn_end = curn + topicName->lenstring.len;

    while (*curf && curn < curn_end)
    {
        if (*curn == '/' && *curf != '/')
            break;
        if (*curf != '+' && *curf != '#' && *curf != *curn)
            break;
        if (*curf == '+')
        {   // skip until we meet the next separator, or end of string
            char* nextpos = curn + 1;
            while (nextpos < curn_end && *nextpos != '/')
                nextpos = ++curn + 1;
        }
        else if (*curf == '#')
            curn = curn_end - 1;    // skip until end of string
        curf++;
        curn++;
    };

    return (curn == curn_end) && (*curf == '\0');
}


static int deliver_message(mqtt_client_t* c, mqtt_string_t* topicName, mqtt_message_t* message)
{
    int i;
    int rc = MQTT_FAILURE;

    // we have to find the right message handler - indexed by topic
    for (i = 0; i < MQTT_MAX_MESSAGE_HANDLERS; ++i)
    {
        if (c->messageHandlers[i].topicFilter != 0 && (mqtt_packet_equals(topicName, (char*)c->messageHandlers[i].topicFilter) ||
                is_topic_matched((char*)c->messageHandlers[i].topicFilter, topicName)))
        {
            if (c->messageHandlers[i].fp != NULL)
            {
                mqtt_message_data_t md;
                new_message_data(&md, topicName, message);
                c->messageHandlers[i].fp(&md);
                rc = MQTT_SUCCESS;
            }
        }
    }

    if (rc == MQTT_FAILURE && c->defaultMessageHandler != NULL)
    {
        mqtt_message_data_t md;
        new_message_data(&md, topicName, message);
        c->defaultMessageHandler(&md);
        rc = MQTT_SUCCESS;
    }

    return rc;
}


static int keepalive(mqtt_client_t* c)
{
    int rc = MQTT_SUCCESS;

    if (c->keepAliveInterval == 0)
    {
        rc = MQTT_SUCCESS;
        goto exit;
    }

    if (mqtt_timer_expired(&(c->ping_timer)))
    {
        if (c->ping_outstanding)
        {
            // if ping failure accumulated above MAX_FAIL_ALLOWED, the connection is broken
            ++(c->fail_count);
            if (c->fail_count >= MQTT_MAX_FAIL_ALLOWED)
            {
                rc = MQTT_DISCONNECTED;
                goto exit;
            }
        }
        else
        {
            mqtt_timer_t timer;
            mqtt_timer_init(&timer);
            mqtt_timer_countdown_ms(&timer, 1000);
            c->ping_outstanding = 1;
            int len = mqtt_serialize_pingreq(c->buf, c->buf_size);
            if (len > 0)
                send_packet(c, len, &timer);
        }
        // re-arm ping counter
        mqtt_timer_countdown(&(c->ping_timer), c->keepAliveInterval);
    }

exit:
    return rc;
}


static int cycle(mqtt_client_t* c, mqtt_timer_t* timer)
{
    // read the socket, see what work is due
    int packet_type = read_packet(c, timer);

    int len = 0,
        rc = MQTT_SUCCESS;

    switch (packet_type)
    {
        case MQTTPACKET_CONNACK:
        case MQTTPACKET_PUBACK:
        case MQTTPACKET_SUBACK:
            break;
        case MQTTPACKET_PUBLISH:
        {
            mqtt_string_t topicName;
            mqtt_message_t msg;
            if (mqtt_deserialize_publish((unsigned char*)&msg.dup, (int*)&msg.qos, (unsigned char*)&msg.retained, (unsigned short*)&msg.id, &topicName,
               (unsigned char**)&msg.payload, (int*)&msg.payloadlen, c->readbuf, c->readbuf_size) != 1)
                goto exit;
            deliver_message(c, &topicName, &msg);
            if (msg.qos != MQTT_QOS0)
            {
                if (msg.qos == MQTT_QOS1)
                    len = mqtt_serialize_ack(c->buf, c->buf_size, MQTTPACKET_PUBACK, 0, msg.id);
                else if (msg.qos == MQTT_QOS2)
                    len = mqtt_serialize_ack(c->buf, c->buf_size, MQTTPACKET_PUBREC, 0, msg.id);
                if (len <= 0)
                    rc = MQTT_FAILURE;
                else
                    rc = send_packet(c, len, timer);
                if (rc == MQTT_FAILURE)
                    goto exit; // there was a problem
            }
            break;
        }
        case MQTTPACKET_PUBREC:
        {
            unsigned short mypacketid;
            unsigned char dup, type;
            if (mqtt_deserialize_ack(&type, &dup, &mypacketid, c->readbuf, c->readbuf_size) != 1)
                rc = MQTT_FAILURE;
            else if ((len = mqtt_serialize_ack(c->buf, c->buf_size, MQTTPACKET_PUBREL, 0, mypacketid)) <= 0)
                rc = MQTT_FAILURE;
            else if ((rc = send_packet(c, len, timer)) != MQTT_SUCCESS) // send the PUBREL packet
                rc = MQTT_FAILURE; // there was a problem
            if (rc == MQTT_FAILURE)
                goto exit; // there was a problem
            break;
        }
        case MQTTPACKET_PUBCOMP:
            break;
        case MQTTPACKET_PINGRESP:
        {
            c->ping_outstanding = 0;
            c->fail_count = 0;
            break;
        }
        case MQTT_READ_ERROR:
        {
            c->isconnected = 0; // we simulate a disconnect if reading error
            rc = MQTT_DISCONNECTED;  // so that the outer layer will reconnect and recover
            break;
        }
    }
    if (c->isconnected)
        rc = keepalive(c);
exit:
    if (rc == MQTT_SUCCESS)
        rc = packet_type;
    return rc;
}


void  mqtt_client_new(mqtt_client_t* c, mqtt_network_t* network, unsigned int command_timeout_ms, unsigned char* buf, size_t buf_size, unsigned char* readbuf, size_t readbuf_size)
{
    int i;
    c->ipstack = network;

    for (i = 0; i < MQTT_MAX_MESSAGE_HANDLERS; ++i)
        c->messageHandlers[i].topicFilter = 0;
    c->command_timeout_ms = command_timeout_ms;
    c->buf = buf;
    c->buf_size = buf_size;
    c->readbuf = readbuf;
    c->readbuf_size = readbuf_size;
    c->isconnected = 0;
    c->ping_outstanding = 0;
    c->fail_count = 0;
    c->defaultMessageHandler = NULL;
    mqtt_timer_init(&(c->ping_timer));
}


int  mqtt_yield(mqtt_client_t* c, int timeout_ms)
{
    int rc = MQTT_SUCCESS;
    mqtt_timer_t timer;

    mqtt_timer_init(&timer);
    mqtt_timer_countdown_ms(&timer, timeout_ms);
    while (!mqtt_timer_expired(&timer))
    {
        rc = cycle(c, &timer);
        // cycle could return 0 or packet_type or 65535 if nothing is read
        // cycle returns DISCONNECTED only if keepalive() fails.
        if (rc == MQTT_DISCONNECTED)
            break;
        rc = MQTT_SUCCESS;
    }
    return rc;
}


// only used in single-threaded mode where one command at a time is in process
static int waitfor(mqtt_client_t* c, int packet_type, mqtt_timer_t* timer)
{
    int rc = MQTT_FAILURE;

    do
    {
        if (mqtt_timer_expired(timer))
            break; // we timed out
    }
    while ((rc = cycle(c, timer)) != packet_type);

    return rc;
}


int  mqtt_connect(mqtt_client_t* c, mqtt_packet_connect_data_t* options)
{
    mqtt_timer_t connect_timer;
    int rc = MQTT_FAILURE;
    mqtt_packet_connect_data_t default_options = mqtt_packet_connect_data_initializer;
    int len = 0;

    mqtt_timer_init(&connect_timer);
    mqtt_timer_countdown_ms(&connect_timer, c->command_timeout_ms);

    if (c->isconnected) // don't send connect packet again if we are already connected
        goto exit;

    if (options == 0)
        options = &default_options; // set default options if none were supplied

    c->keepAliveInterval = options->keepAliveInterval;
    mqtt_timer_countdown(&(c->ping_timer), c->keepAliveInterval);

    if ((len = mqtt_serialize_connect(c->buf, c->buf_size, options)) <= 0)
        goto exit;
    if ((rc = send_packet(c, len, &connect_timer)) != MQTT_SUCCESS)  // send the connect packet
        goto exit; // there was a problem

    // this will be a blocking call, wait for the connack
    if (waitfor(c, MQTTPACKET_CONNACK, &connect_timer) == MQTTPACKET_CONNACK)
    {
        unsigned char connack_rc = 255;
        char sessionPresent = 0;
        if (mqtt_deserialize_connack((unsigned char*)&sessionPresent, &connack_rc, c->readbuf, c->readbuf_size) == 1)
            rc = connack_rc;
        else
            rc = MQTT_FAILURE;
    }
    else
        rc = MQTT_FAILURE;

exit:
    if (rc == MQTT_SUCCESS)
        c->isconnected = 1;
    return rc;
}


int  mqtt_subscribe(mqtt_client_t* c, const char* topic, enum mqtt_qos qos, mqtt_message_handler_t handler)
{
    int rc = MQTT_FAILURE;
    mqtt_timer_t timer;
    int len = 0;
    mqtt_string_t topicStr = mqtt_string_initializer;
    topicStr.cstring = (char *)topic;

    mqtt_timer_init(&timer);
    mqtt_timer_countdown_ms(&timer, c->command_timeout_ms);
    if (!c->isconnected)
        goto exit;

    len = mqtt_serialize_subscribe(c->buf, c->buf_size, 0, get_next_packet_id(c), 1, &topicStr, (int*)&qos);
    if (len <= 0)
        goto exit;
    if ((rc = send_packet(c, len, &timer)) != MQTT_SUCCESS) // send the subscribe packet
    {
        goto exit;             // there was a problem
    }

    if (waitfor(c, MQTTPACKET_SUBACK, &timer) == MQTTPACKET_SUBACK)      // wait for suback
    {
        int count = 0, grantedQoS = -1;
        unsigned short mypacketid;
        if (mqtt_deserialize_suback(&mypacketid, 1, &count, &grantedQoS, c->readbuf, c->readbuf_size) == 1)
            rc = grantedQoS; // 0, 1, 2 or 0x80
        if (rc != 0x80)
        {
            int i;
            
            rc = MQTT_FAILURE;
            for (i = 0; i < MQTT_MAX_MESSAGE_HANDLERS; ++i)
            {
                if (c->messageHandlers[i].topicFilter == 0)
                {
                    c->messageHandlers[i].topicFilter = topic;
                    c->messageHandlers[i].fp = handler;
                    rc = 0;
                    break;
                }
            }
        }
    }
    else
        rc = MQTT_FAILURE;

exit:
    return rc;
}


int  mqtt_unsubscribe(mqtt_client_t* c, const char* topicFilter)
{
    int rc = MQTT_FAILURE;
    mqtt_timer_t timer;
    mqtt_string_t topic = mqtt_string_initializer;
    topic.cstring = (char *)topicFilter;
    int len = 0;

    mqtt_timer_init(&timer);
    mqtt_timer_countdown_ms(&timer, c->command_timeout_ms);

    if (!c->isconnected)
        goto exit;

    if ((len = mqtt_serialize_unsubscribe(c->buf, c->buf_size, 0, get_next_packet_id(c), 1, &topic)) <= 0)
        goto exit;
    if ((rc = send_packet(c, len, &timer)) != MQTT_SUCCESS) // send the subscribe packet
        goto exit; // there was a problem

    if (waitfor(c, MQTTPACKET_UNSUBACK, &timer) == MQTTPACKET_UNSUBACK)
    {
        unsigned short mypacketid;  // should be the same as the packetid above
        if (mqtt_deserialize_unsuback(&mypacketid, c->readbuf, c->readbuf_size) == 1)
            rc = 0;
    }
    else
        rc = MQTT_FAILURE;

exit:
    return rc;
}


int  mqtt_publish(mqtt_client_t* c, const char* topic, mqtt_message_t* message)
{
    int rc = MQTT_FAILURE;
    mqtt_timer_t timer;
    mqtt_string_t topicStr = mqtt_string_initializer;
    topicStr.cstring = (char *)topic;
    int len = 0;

    mqtt_timer_init(&timer);
    mqtt_timer_countdown_ms(&timer, c->command_timeout_ms);

    if (!c->isconnected)
        goto exit;

    if (message->qos == MQTT_QOS1 || message->qos == MQTT_QOS2)
        message->id = get_next_packet_id(c);

    len = mqtt_serialize_publish(c->buf, c->buf_size, 0, message->qos, message->retained, message->id, 
              topicStr, (unsigned char*)message->payload, message->payloadlen);
    if (len <= 0)
        goto exit;
    if ((rc = send_packet(c, len, &timer)) != MQTT_SUCCESS) // send the subscribe packet
    {
        goto exit; // there was a problem
    }

    if (message->qos == MQTT_QOS1)
    {
        if (waitfor(c, MQTTPACKET_PUBACK, &timer) == MQTTPACKET_PUBACK)
        {
            // We still can receive from broker, treat as recoverable
            c->fail_count = 0;
            unsigned short mypacketid;
            unsigned char dup, type;
            if (mqtt_deserialize_ack(&type, &dup, &mypacketid, c->readbuf, c->readbuf_size) != 1)
                rc = MQTT_FAILURE;
            else
                rc = MQTT_SUCCESS;
        }
        else
        {
            rc = MQTT_FAILURE;
        }

    }
    else if (message->qos == MQTT_QOS2)
    {
        if (waitfor(c, MQTTPACKET_PUBCOMP, &timer) == MQTTPACKET_PUBCOMP)
        {
            // We still can receive from broker, treat as recoverable
            c->fail_count = 0;
            unsigned short mypacketid;
            unsigned char dup, type;
            if (mqtt_deserialize_ack(&type, &dup, &mypacketid, c->readbuf, c->readbuf_size) != 1)
                rc = MQTT_FAILURE;
            else
                rc = MQTT_SUCCESS;
        }
        else
        {
            rc = MQTT_FAILURE;
        }
    }

exit:
    return rc;
}


int  mqtt_disconnect(mqtt_client_t* c)
{
    int rc = MQTT_FAILURE;
    mqtt_timer_t timer;     // we might wait for incomplete incoming publishes to complete
    int len = mqtt_serialize_disconnect(c->buf, c->buf_size);

    mqtt_timer_init(&timer);
    mqtt_timer_countdown_ms(&timer, c->command_timeout_ms);

    if (len > 0)
        rc = send_packet(c, len, &timer);            // send the disconnect packet

    c->isconnected = 0;
    return rc;
}

