/*
 * Configuration parameters.
 *
 * Copyright (C) 2016, 2017 OurAirQuality.org
 *
 * Licensed under the Apache License, Version 2.0, January 2004 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE CONTRIBUTORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS WITH THE SOFTWARE.
 *
 */

#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "sysparam.h"

/*
 * Parameters.
 */
uint8_t param_leds;
uint8_t param_pms_uart;
uint8_t param_i2c_scl;
uint8_t param_i2c_sda;
uint8_t param_logging;
char *param_web_server;
char param_web_port[7];
char *param_web_path;
uint32_t param_sensor_id;
uint32_t param_key_size;
uint8_t *param_sha3_key;

void init_params()
{
    sysparam_status_t status;

    param_leds = 1;
    param_pms_uart = 2;
    param_i2c_scl = 5;
    param_i2c_sda = 4;
    param_logging = 1;
    param_web_server = NULL;
    bzero(param_web_port, sizeof(param_web_port));
    param_web_path = NULL;
    param_sensor_id = 0;
    param_key_size = 0;
    param_sha3_key = NULL;

    sysparam_get_int8("oaq_leds", (int8_t *)&param_leds);
    sysparam_get_int8("oaq_pms_uart", (int8_t *)&param_pms_uart);
    sysparam_get_int8("oaq_i2c_scl", (int8_t *)&param_i2c_scl);
    sysparam_get_int8("oaq_i2c_sda", (int8_t *)&param_i2c_sda);

    sysparam_get_int8("oaq_logging", (int8_t *)&param_logging);

    sysparam_get_string("oaq_web_server", &param_web_server);
    int32_t port = 80;
    sysparam_get_int32("oaq_web_port", &port);
    snprintf(param_web_port, sizeof(param_web_port), "%u", port);
    sysparam_get_string("oaq_web_path", &param_web_path);

    sysparam_get_int32("oaq_sensor_id", (int32_t *)&param_sensor_id);
    status = sysparam_get_data("oaq_sha3_key", &param_sha3_key, &param_key_size, NULL);
    if (status != SYSPARAM_OK) {
        param_key_size = 0;
        param_sha3_key = NULL;
    }
}
