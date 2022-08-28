/* Part of esp-open-rtos
 * BSD Licensed as described in the file LICENSE
 */
#include <stdio.h>
#include <stdlib.h>

void * __attribute__((weak)) operator new(size_t size)
{
    return malloc(size);
}

void * __attribute__((weak)) operator new[](size_t size)
{
    return malloc(size);
}

void __attribute__((weak)) operator delete(void * ptr)
{
    free(ptr);
}

void __attribute__((weak)) operator delete[](void * ptr)
{
    free(ptr);
}
