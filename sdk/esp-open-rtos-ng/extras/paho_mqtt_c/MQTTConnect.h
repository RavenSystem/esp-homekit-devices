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

#ifndef MQTTCONNECT_H_
#define MQTTCONNECT_H_

#if !defined(DLLImport)
  #define DLLImport
#endif
#if !defined(DLLExport)
  #define DLLExport
#endif


typedef union
{
	unsigned char all;	/**< all connect flags */
#if defined(REVERSED)
	struct
	{
		unsigned int username : 1;			/**< 3.1 user name */
		unsigned int password : 1; 			/**< 3.1 password */
		unsigned int willRetain : 1;		/**< will retain setting */
		unsigned int willQoS : 2;				/**< will QoS value */
		unsigned int will : 1;			    /**< will flag */
		unsigned int cleansession : 1;	  /**< clean session flag */
		unsigned int : 1;	  	          /**< unused */
	} bits;
#else
	struct
	{
		unsigned int : 1;	     					/**< unused */
		unsigned int cleansession : 1;	  /**< cleansession flag */
		unsigned int will : 1;			    /**< will flag */
		unsigned int willQoS : 2;				/**< will QoS value */
		unsigned int willRetain : 1;		/**< will retain setting */
		unsigned int password : 1; 			/**< 3.1 password */
		unsigned int username : 1;			/**< 3.1 user name */
	} bits;
#endif
} mqtt_connect_flags_t;	/**< connect flags byte */



/**
 * Defines the MQTT "Last Will and Testament" (LWT) settings for
 * the connect packet.
 */
typedef struct
{
	/** The eyecatcher for this structure.  must be MQTW. */
	char struct_id[4];
	/** The version number of this structure.  Must be 0 */
	int struct_version;
	/** The LWT topic to which the LWT message will be published. */
	mqtt_string_t topicName;
	/** The LWT payload. */
	mqtt_string_t message;
	/**
      * The retained flag for the LWT message (see MQTTAsync_message.retained).
      */
	unsigned char retained;
	/**
      * The quality of service setting for the LWT message (see
      * MQTTAsync_message.qos and @ref qos).
      */
	char qos;
} mqtt_packet_will_options_t;


#define mqtt_packet_will_options_initializer { {'M', 'Q', 'T', 'W'}, 0, {NULL, {0, NULL}}, {NULL, {0, NULL}}, 0, 0 }


typedef struct
{
	/** The eyecatcher for this structure.  must be MQTC. */
	char struct_id[4];
	/** The version number of this structure.  Must be 0 */
	int struct_version;
	/** Version of MQTT to be used.  3 = 3.1 4 = 3.1.1
	  */
	unsigned char MQTTVersion;
	mqtt_string_t clientID;
	unsigned short keepAliveInterval;
	unsigned char cleansession;
	unsigned char willFlag;
	mqtt_packet_will_options_t will;
	mqtt_string_t username;
	mqtt_string_t password;
} mqtt_packet_connect_data_t;

typedef union
{
	unsigned char all;	/**< all connack flags */
#if defined(REVERSED)
	struct
	{
		unsigned int sessionpresent : 1;    /**< session present flag */
		unsigned int : 7;	  	          /**< unused */
	} bits;
#else
	struct
	{
		unsigned int : 7;	     			/**< unused */
		unsigned int sessionpresent : 1;    /**< session present flag */
	} bits;
#endif
} mqtt_conn_ack_flags_t;	/**< connack flags byte */

#define mqtt_packet_connect_data_initializer { {'M', 'Q', 'T', 'C'}, 0, 4, {NULL, {0, NULL}}, 60, 1, 0, \
		mqtt_packet_will_options_initializer, {NULL, {0, NULL}}, {NULL, {0, NULL}} }

DLLExport int mqtt_serialize_connect(unsigned char* buf, int buflen, mqtt_packet_connect_data_t* options);
DLLExport int mqtt_deserialize_connect(mqtt_packet_connect_data_t* data, unsigned char* buf, int len);

DLLExport int mqtt_serialize_connack(unsigned char* buf, int buflen, unsigned char connack_rc, unsigned char sessionPresent);
DLLExport int mqtt_deserialize_connack(unsigned char* sessionPresent, unsigned char* connack_rc, unsigned char* buf, int buflen);

DLLExport int mqtt_serialize_disconnect(unsigned char* buf, int buflen);
DLLExport int mqtt_serialize_pingreq(unsigned char* buf, int buflen);

#endif /* MQTTCONNECT_H_ */
