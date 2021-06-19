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

#ifndef ESP_OPEN_RTOS_TIMER_HPP
#define	ESP_OPEN_RTOS_TIMER_HPP

#include "FreeRTOS.h"
#include "task.h"

namespace esp_open_rtos {
namespace timer {

#define __millis()  (xTaskGetTickCount() * portTICK_PERIOD_MS)

/******************************************************************************************************************
 * countdown_t
 *
 */
class countdown_t
{
public:
    /**
     * 
     */
    countdown_t()
    {  
	interval_end_ms = 0L;
    }
    /**
     * 
     * @param ms
     */
    countdown_t(int ms)
    {
        countdown_ms(ms);   
    }
    /**
     * 
     * @return 
     */
    bool expired()
    {
        return (interval_end_ms > 0L) && (__millis() >= interval_end_ms);
    }
    /**
     * 
     * @param ms
     */
    void countdown_ms(unsigned long ms)  
    {
        interval_end_ms = __millis() + ms;
    }
    /**
     * 
     * @param seconds
     */
    void countdown(int seconds)
    {
        countdown_ms((unsigned long)seconds * 1000L);
    }
    /**
     * 
     * @return 
     */
    int left_ms()
    {
        return interval_end_ms - __millis();
    }
    
private:
    TickType_t interval_end_ms;
};

} // namespace timer {
} // namespace esp_open_rtos {

#endif
