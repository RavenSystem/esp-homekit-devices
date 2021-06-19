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

#ifndef ESP_OPEN_RTOS_TASK_HPP
#define	ESP_OPEN_RTOS_TASK_HPP

#include "FreeRTOS.h"
#include "task.h"

namespace esp_open_rtos {
namespace thread {

/******************************************************************************************************************
 * task_t
 *
 */
class task_t
{
public:
    /**
     * 
     */
    task_t()
    {}
    /**
     * 
     * @param pcName
     * @param usStackDepth
     * @param uxPriority
     * @return 
     */
    int task_create(const char* const pcName, unsigned short usStackDepth = 256, unsigned portBASE_TYPE uxPriority = 2)
    {
        return xTaskCreate(task_t::_task, pcName, usStackDepth, this, uxPriority, NULL);
    }
    
protected:
    /**
     * 
     * @param ms
     */
    void sleep(unsigned long ms)
    {
        vTaskDelay(ms / portTICK_PERIOD_MS);
    }
    /**
     * 
     * @return 
     */
    inline unsigned long millis()
    {
        return xTaskGetTickCount() * portTICK_PERIOD_MS;
    }
    
private:
    /**
     * 
     */
    virtual void task() = 0;
    /**
     * 
     * @param pvParameters
     */
    static void _task(void* pvParameters)
    {
        if(pvParameters != 0) {
            ((task_t*)(pvParameters))->task();
        }
    }
    
    // no copy and no = operator
    task_t(const task_t&);
    task_t &operator=(const task_t&);    
};

} //namespace thread {
} //namespace esp_open_rtos {


#endif	/* ESP_OPEN_RTOS_TASK_HPP */

