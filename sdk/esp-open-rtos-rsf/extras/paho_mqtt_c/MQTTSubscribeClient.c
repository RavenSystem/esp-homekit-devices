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
#include <espressif/esp_common.h>

#include "MQTTPacket.h"
#include "StackTrace.h"

#include <string.h>

/**
  * Determines the length of the MQTT subscribe packet that would be produced using the supplied parameters
  * @param count the number of topic filter strings in topicFilters
  * @param topicFilters the array of topic filter strings to be used in the publish
  * @return the length of buffer needed to contain the serialized version of the packet
  */
static int subscribe_length(int count, mqtt_string_t topicFilters[])
{
	int i;
	int len = 2; /* packetid */

	for (i = 0; i < count; ++i)
		len += 2 + mqtt_strlen(topicFilters[i]) + 1; /* length + topic + req_qos */
	return len;
}


/**
  * Serializes the supplied subscribe data into the supplied buffer, ready for sending
  * @param buf the buffer into which the packet will be serialized
  * @param buflen the length in bytes of the supplied bufferr
  * @param dup integer - the MQTT dup flag
  * @param packetid integer - the MQTT packet identifier
  * @param count - number of members in the topicFilters and reqQos arrays
  * @param topicFilters - array of topic filter names
  * @param requestedQoSs - array of requested QoS
  * @return the length of the serialized data.  <= 0 indicates error
  */
int  mqtt_serialize_subscribe(unsigned char* buf, int buflen, unsigned char dup, unsigned short packetid, int count,
		mqtt_string_t topicFilters[], int requestedQoSs[])
{
	unsigned char *ptr = buf;
	mqtt_header_t header = {0};
	int rem_len = 0;
	int rc = 0;
	int i = 0;

	FUNC_ENTRY;
	if (mqtt_packet_len(rem_len = subscribe_length(count, topicFilters)) > buflen)
	{
		rc = MQTTPACKET_BUFFER_TOO_SHORT;
		goto exit;
	}

	header.byte = 0;
	header.bits.type = MQTTPACKET_SUBSCRIBE;
	header.bits.dup = dup;
	header.bits.qos = 1;
	mqtt_write_char(&ptr, header.byte); /* write header */

	ptr += mqtt_packet_encode(ptr, rem_len); /* write remaining length */;

	mqtt_write_int(&ptr, packetid);

	for (i = 0; i < count; ++i)
	{
		mqtt_write_mqqt_str(&ptr, topicFilters[i]);
		mqtt_write_char(&ptr, requestedQoSs[i]);
	}

	rc = ptr - buf;
exit:
	FUNC_EXIT_RC(rc);
	return rc;
}



/**
  * Deserializes the supplied (wire) buffer into suback data
  * @param packetid returned integer - the MQTT packet identifier
  * @param maxcount - the maximum number of members allowed in the grantedQoSs array
  * @param count returned integer - number of members in the grantedQoSs array
  * @param grantedQoSs returned array of integers - the granted qualities of service
  * @param buf the raw buffer data, of the correct length determined by the remaining length field
  * @param buflen the length in bytes of the data in the supplied buffer
  * @return error code.  1 is success, 0 is failure
  */
int  mqtt_deserialize_suback(unsigned short* packetid, int maxcount, int* count, int grantedQoSs[], unsigned char* buf, int buflen)
{
	mqtt_header_t header = {0};
	unsigned char* curdata = buf;
	unsigned char* enddata = NULL;
	int rc = 0;
	int mylen;

	FUNC_ENTRY;
	header.byte = mqtt_read_char(&curdata);
	if (header.bits.type != MQTTPACKET_SUBACK)
		goto exit;

	curdata += (rc = mqtt_packet_decode_buf(curdata, &mylen)); /* read remaining length */
	enddata = curdata + mylen;
	if (enddata - curdata < 2)
		goto exit;

	*packetid = mqtt_read_int(&curdata);

	*count = 0;
	while (curdata < enddata)
	{
		if (*count > maxcount)
		{
			rc = -1;
			goto exit;
		}
		grantedQoSs[(*count)++] = mqtt_read_char(&curdata);
	}

	rc = 1;
exit:
	FUNC_EXIT_RC(rc);
	return rc;
}


