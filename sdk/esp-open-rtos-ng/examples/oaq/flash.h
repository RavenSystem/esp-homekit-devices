/*
 * Write the memory resident buffers to flash.
 *
 * Copyright (C) 2016, 2017 OurAirQuality.org
 *
 * Licensed under the Apache License, Version 2.0, January 2004 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
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

uint32_t get_buffer_to_post(uint32_t *index, uint32_t *start, uint8_t *buf);
void note_buffer_posted(uint32_t index, uint32_t size);
uint32_t maybe_buffer_to_post(void);
void clear_maybe_buffer_to_post(void);

uint32_t init_flash(void);
void flash_data(void *pvParameters);

extern TaskHandle_t flash_data_task;

uint32_t get_buffer_size(uint32_t requested_index, uint32_t *index, uint32_t *next_index, bool *sealed);
bool get_buffer_range(uint32_t index, uint32_t start, uint32_t end, uint8_t *buf);
bool erase_flash_data(void);
