#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <stdlib.h>
#include <stdio.h>
#include "adv_logger.h"

typedef unsigned char byte;

#ifdef HOMEKIT_DEBUG

#define DEBUG(message, ...)         printf("%s: " message "\n", __func__, ##__VA_ARGS__)

#else

#define DEBUG(message, ...)

#endif

#ifdef ESP_PLATFORM

#define DEBUG_HEAP()                DEBUG("Free heap: %i", esp_get_free_heap_size());
#define INFO(message, ...)          adv_logger_printf(message "\n", ##__VA_ARGS__)
#define ERROR(message, ...)         adv_logger_printf("! " message "\n", ##__VA_ARGS__)

#else

#define DEBUG_HEAP()                DEBUG("Free heap: %i", xPortGetFreeHeapSize());
#define INFO(message, ...)          printf(message "\n", ##__VA_ARGS__)
#define ERROR(message, ...)         printf("! " message "\n", ##__VA_ARGS__)

#endif

char *binary_to_string(const byte *data, unsigned int size);
void print_binary(const char *prompt, const byte *data, unsigned int size);

#endif // __DEBUG_H__
