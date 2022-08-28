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

#ifndef MQTTUNSUBSCRIBE_H_
#define MQTTUNSUBSCRIBE_H_

#if !defined(DLLImport)
  #define DLLImport
#endif
#if !defined(DLLExport)
  #define DLLExport
#endif

DLLExport int mqtt_serialize_unsubscribe(unsigned char* buf, int buflen, unsigned char dup, unsigned short packetid,
		int count, mqtt_string_t topicFilters[]);

DLLExport int mqtt_deserialize_unsubscribe(unsigned char* dup, unsigned short* packetid, int max_count, int* count, mqtt_string_t topicFilters[],
		unsigned char* buf, int len);

DLLExport int mqtt_serialize_unsuback(unsigned char* buf, int buflen, unsigned short packetid);

DLLExport int mqtt_deserialize_unsuback(unsigned short* packetid, unsigned char* buf, int len);

#endif /* MQTTUNSUBSCRIBE_H_ */
