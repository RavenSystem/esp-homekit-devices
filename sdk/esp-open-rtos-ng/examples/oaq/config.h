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

/*
 * Parameters.
 *
 * The 'leds' can be 0 to disable use of LEDS, 1 for Nodemcu, and 2 for
 * Witty.
 */
extern uint8_t param_leds;

/*
 * The PMS*003 serial port:
 *  0 - None, disabled (default).
 *  1 - UART0 on GPIO3 aka RX (Nodemcu pin D9).
 *  2 - UART0 swapped pins mode, GPIO13 (Nodemcu pin D7).
 *  3 - TODO Flipping between the above for two sensors?
 */
extern uint8_t param_pms_uart;

/*
 * I2C bus pin definitions, GPIO numbers.
 *
 * SCL defaults to GPIO 5 (Nodemcu pin D1) and SDA to GPIO 4 (Nodemcu
 * pin D2) if not supplied.
 */
extern uint8_t param_i2c_scl;
extern uint8_t param_i2c_sda;

/*
 * Logging to the data buffers can be disabled by clearing this variable, and
 * this is the start of the data flow so it stops more data entering, but it
 * continues to trigger writes to flush the pipeline.
 */
extern bool param_logging;

/*
 * Network parameters. If not sufficiently initialized to communicate with a
 * server then wifi is disabled and the post-data task is not created.
 */

extern char *param_web_server;
extern char param_web_port[];
extern char *param_web_path;
extern uint32_t param_sensor_id;
extern uint32_t param_key_size;
extern uint8_t *param_sha3_key;

void init_params();


