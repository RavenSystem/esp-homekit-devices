/* 
 * The MIT License (MIT)
 * 
 * ESP8266 FreeRTOS Firmware
 * Copyright (c) 2015 Michael Jacobsen (github.com/mikejac)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * https://github.com/SuperHouse/esp-open-rtos
 * 
 */

#ifndef ESP_OPEN_RTOS_QUEUE_HPP
#define	ESP_OPEN_RTOS_QUEUE_HPP

#include "FreeRTOS.h"
#include "queue.h"

namespace esp_open_rtos {
namespace thread {

/******************************************************************************************************************
 * class queue_t
 *
 */
template<class Data>
class queue_t
{
public:
    /**
     * 
     */
    inline queue_t()
    {
        queue = 0;
    }
    /**
     * 
     * @param uxQueueLength
     * @param uxItemSize
     * @return 
     */
    inline int queue_create(unsigned portBASE_TYPE uxQueueLength)
    {
        queue = xQueueCreate(uxQueueLength, sizeof(Data));
        
        if(queue == NULL) {
            return -1;
        }
        else {
            return 0;
        }
    }
    /**
     * 
     */
    inline void queue_destroy()
    {
        vQueueDelete(queue);
        queue = 0;
    }
    /**
     * 
     * @param data
     * @param ms
     * @return 
     */
    inline int post(const Data& data, unsigned long ms = 0)
    {
        return (xQueueSend(queue, &data, ms / portTICK_PERIOD_MS) == pdTRUE) ? 0 : -1;
    }
    /**
     * 
     * @param data
     * @param ms
     * @return 
     */
    inline int receive(Data& data, unsigned long ms = 0)
    {
        return (xQueueReceive(queue, &data, ms / portTICK_PERIOD_MS) == pdTRUE) ? 0 : -1;
    }
    /**
     * 
     * @param other
     * @return 
     */
    const queue_t &operator = (const queue_t& other)
    {
        if(this != &other) {        // protect against invalid self-assignment
            queue = other.queue;
        }
        
        return *this;
    }

private:
    QueueHandle_t queue;

    // Disable copy construction.
    queue_t (const queue_t&);
};

} //namespace thread {
} //namespace esp_open_rtos {


#endif	/* ESP_OPEN_RTOS_QUEUE_HPP */

