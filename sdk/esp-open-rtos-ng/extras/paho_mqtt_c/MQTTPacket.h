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
 *    Xiang Rong - 442039 Add makefile to Embedded C client
 *******************************************************************************/

#ifndef MQTTPACKET_H_
#define MQTTPACKET_H_

#if defined(__cplusplus) /* If this is a C++ compiler, use C linkage */
extern "C" {
#endif

#if defined(WIN32_DLL) || defined(WIN64_DLL)
  #define DLLImport __declspec(dllimport)
  #define DLLExport __declspec(dllexport)
#elif defined(LINUX_SO)
  #define DLLImport extern
  #define DLLExport  __attribute__ ((visibility ("default")))
#else
  #define DLLImport
  #define DLLExport 
#endif

enum errors
{
	MQTTPACKET_BUFFER_TOO_SHORT = -2,
	MQTTPACKET_READ_ERROR = -1,
	MQTTPACKET_READ_COMPLETE
};

enum msgTypes
{
	MQTTPACKET_CONNECT = 1,
	MQTTPACKET_CONNACK,
	MQTTPACKET_PUBLISH,
	MQTTPACKET_PUBACK,
	MQTTPACKET_PUBREC,
	MQTTPACKET_PUBREL,
	MQTTPACKET_PUBCOMP,
	MQTTPACKET_SUBSCRIBE,
	MQTTPACKET_SUBACK,
	MQTTPACKET_UNSUBSCRIBE,
	MQTTPACKET_UNSUBACK,
	MQTTPACKET_PINGREQ,
	MQTTPACKET_PINGRESP,
	MQTTPACKET_DISCONNECT
};

/**
 * Bitfields for the MQTT header byte.
 */
typedef union
{
	unsigned char byte;	                /**< the whole byte */
#if defined(REVERSED)
	struct
	{
		unsigned int type : 4;			/**< message type nibble */
		unsigned int dup : 1;				/**< DUP flag bit */
		unsigned int qos : 2;				/**< QoS value, 0, 1 or 2 */
		unsigned int retain : 1;		/**< retained flag bit */
	} bits;
#else
	struct
	{
		unsigned int retain : 1;		/**< retained flag bit */
		unsigned int qos : 2;				/**< QoS value, 0, 1 or 2 */
		unsigned int dup : 1;				/**< DUP flag bit */
		unsigned int type : 4;			/**< message type nibble */
	} bits;
#endif
} mqtt_header_t;

typedef struct
{
	int len;
	char* data;
} mqtt_string_len_t;

typedef struct
{
	char* cstring;
	mqtt_string_len_t lenstring;
} mqtt_string_t;

#define mqtt_string_initializer {NULL, {0, NULL}}

int mqtt_strlen(mqtt_string_t mqttstring);

#include "MQTTConnect.h"
#include "MQTTPublish.h"
#include "MQTTSubscribe.h"
#include "MQTTUnsubscribe.h"
#include "MQTTFormat.h"

int mqtt_serialize_ack(unsigned char* buf, int buflen, unsigned char type, unsigned char dup, unsigned short packetid);
int mqtt_deserialize_ack(unsigned char* packettype, unsigned char* dup, unsigned short* packetid, unsigned char* buf, int buflen);

int mqtt_packet_len(int rem_len);
int mqtt_packet_equals(mqtt_string_t* a, char* b);

int mqtt_packet_encode(unsigned char* buf, int length);
int mqtt_packet_decode(int (*getcharfn)(unsigned char*, int), int* value);
int mqtt_packet_decode_buf(unsigned char* buf, int* value);

int mqtt_read_int(unsigned char** pptr);
char mqtt_read_char(unsigned char** pptr);
void mqtt_write_char(unsigned char** pptr, char c);
void mqtt_write_int(unsigned char** pptr, int anInt);
int mqtt_read_str_len(mqtt_string_t* mqttstring, unsigned char** pptr, unsigned char* enddata);
void mqtt_write_cstr(unsigned char** pptr, const char* string);
void mqtt_write_mqqt_str(unsigned char** pptr, mqtt_string_t mqttstring);

DLLExport int mqtt_packet_read(unsigned char* buf, int buflen, int (*getfn)(unsigned char*, int));

typedef struct {
	int (*getfn)(void *, unsigned char*, int); /* must return -1 for error, 0 for call again, or the number of bytes read */
	void *sck;	/* pointer to whatever the system may use to identify the transport */
	int multiplier;
	int rem_len;
	int len;
	char state;
} mqtt_transport_t;

int mqtt_packet_readnb(unsigned char* buf, int buflen, mqtt_transport_t *trp);

#ifdef __cplusplus /* If this is a C++ compiler, use C linkage */
}
#endif


#endif /* MQTTPACKET_H_ */
