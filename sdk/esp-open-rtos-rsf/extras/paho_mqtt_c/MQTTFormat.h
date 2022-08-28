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
 *    Ian Craggs - initial API and implementation and/or initial documentation
 *******************************************************************************/

#if !defined(MQTTFORMAT_H)
#define MQTTFORMAT_H

#include "StackTrace.h"
#include "MQTTPacket.h"

const char* mqtt_packet_get_name(unsigned short packetid);
int mqtt_string_format_connect(char* strbuf, int strbuflen, mqtt_packet_connect_data_t* data);
int mqtt_string_format_connack(char* strbuf, int strbuflen, unsigned char connack_rc, unsigned char sessionPresent);
int mqtt_string_format_publish(char* strbuf, int strbuflen, unsigned char dup, int qos, unsigned char retained,
		unsigned short packetid, mqtt_string_t topicName, unsigned char* payload, int payloadlen);
int mqtt_string_format_ack(char* strbuf, int strbuflen, unsigned char packettype, unsigned char dup, unsigned short packetid);
int mqtt_string_format_subscribe(char* strbuf, int strbuflen, unsigned char dup, unsigned short packetid, int count,
		mqtt_string_t topicFilters[], int requestedQoSs[]);
int mqtt_string_format_suback(char* strbuf, int strbuflen, unsigned short packetid, int count, int* grantedQoSs);
int mqtt_string_format_unsubscribe(char* strbuf, int strbuflen, unsigned char dup, unsigned short packetid,
		int count, mqtt_string_t topicFilters[]);
char* mqtt_format_to_client_string(char* strbuf, int strbuflen, unsigned char* buf, int buflen);
char* mqtt_format_to_server_string(char* strbuf, int strbuflen, unsigned char* buf, int buflen);

#endif
