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

#ifndef ESP_OPEN_RTOS_MUTEX_HPP
#define	ESP_OPEN_RTOS_MUTEX_HPP

#include "semphr.h"

namespace esp_open_rtos {
namespace thread {

/******************************************************************************************************************
 * class mutex_t
 *
 */
class mutex_t
{
public:
    /**
     * 
     */
    inline mutex_t()
    {
        mutex = 0;
    }
    /**
     * 
     * @return 
     */
    inline int mutex_create()
    {
        mutex = xSemaphoreCreateMutex();        
        
        if(mutex == NULL) {
            return -1;
        }
        else {
            return 0;
        }
    }
    /**
     * 
     */
    inline void mutex_destroy()
    {
        vQueueDelete(mutex);
        mutex = 0;
    }
    /**
     * 
     * @return 
     */
    inline int lock()
    {
        return (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) ? 0 : -1;
    }
    /**
     * 
     * @param ms
     * @return 
     */
    inline int try_lock(unsigned long ms)
    {
        return (xSemaphoreTake(mutex, ms / portTICK_PERIOD_MS) == pdTRUE) ? 0 : -1;
    }
    /**
     * 
     * @return 
     */
    inline int unlock()
    {
        return (xSemaphoreGive(mutex) == pdTRUE) ? 0 : -1;
    }

private:
    SemaphoreHandle_t    mutex;

    // Disable copy construction and assignment.
    mutex_t (const mutex_t&);
    const mutex_t &operator = (const mutex_t&);
};

} //namespace thread {
} //namespace esp_open_rtos {


#endif	/* ESP_OPEN_RTOS_MUTEX_HPP */

