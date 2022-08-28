/* A very basic C++ example, really just proof of concept for C++

   This sample code is in the public domain.
 */
#include "espressif/esp_common.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include <esp/uart.h>

class Counter
{
private:
  uint32_t _count;
public:
  Counter(uint32_t initial_count)
  {
    this->_count = initial_count;
    printf("Counter initialised with count %d\r\n", initial_count);
  }

  void Increment()
  {
    _count++;
  }

  uint32_t getCount()
  {
    return _count;
  }
};

static Counter static_counter(99);

void task1(void *pvParameters)
{
  Counter local_counter = Counter(12);
  Counter *new_counter = new Counter(24);
  while(1) {
    Counter *counter = NULL;
    switch(rand() % 3) {
    case 0:
      counter = &local_counter;
      break;
    case 1:
      counter = &static_counter;
      break;
    default:
      counter = new_counter;
      break;
    }
    counter->Increment();
    printf("local counter %d static counter %d newly allocated counter %d\r\n", local_counter.getCount(),
	   static_counter.getCount(), new_counter->getCount());
    vTaskDelay(100);
  }
}

extern "C" void user_init(void)
{
    uart_set_baud(0, 115200);
    printf("SDK version:%s\n", sdk_system_get_sdk_version());
    xTaskCreate(task1, "tsk1", 256, NULL, 2, NULL);
}
