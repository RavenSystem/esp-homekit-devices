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

#include "task.hpp"
#include "queue.hpp"

#include "espressif/esp_common.h"

#include "esp/uart.h"

/******************************************************************************************************************
 * task_1_t
 *
 */
class task_1_t: public esp_open_rtos::thread::task_t
{
public:
    esp_open_rtos::thread::queue_t<uint32_t> queue;
    
private:
    void task()
    {
        printf("task_1_t::task(): start\n");

        uint32_t count = 0;

        while(true) {
            sleep(1000);
            queue.post(count);
            count++;
        }
    }    
};
/******************************************************************************************************************
 * task_2_t
 *
 */
class task_2_t: public esp_open_rtos::thread::task_t
{
public:
    esp_open_rtos::thread::queue_t<uint32_t> queue;
    
private:
    void task()
    {
        printf("task_2_t::task(): start\n");

        while(true) {
            uint32_t count;

            if(queue.receive(count, 1500) == 0) {
                printf("task_2_t::task(): got %u\n", count);
            } 
            else {
                printf("task_2_t::task(): no msg\n");
            }
        }
    }    
};
/******************************************************************************************************************
 * globals
 *
 */
task_1_t task_1;
task_2_t task_2;

esp_open_rtos::thread::queue_t<uint32_t> MyQueue;

/**
 * 
 */
extern "C" void user_init(void)
{
    uart_set_baud(0, 115200);
    
    MyQueue.queue_create(10);
    
    task_1.queue = MyQueue;
    task_2.queue = MyQueue;
    
    task_1.task_create("tsk1");
    task_2.task_create("tsk2");
}
