/* The serial driver depends on counting semaphores */
#define configUSE_COUNTING_SEMAPHORES 1

#define configCHECK_FOR_STACK_OVERFLOW 2

/* IDLE task. */
#define configMINIMAL_STACK_SIZE 192

/* Debugging */
#define configUSE_TRACE_FACILITY 1
#define configGENERATE_RUN_TIME_STATS 1
#define portGET_RUN_TIME_COUNTER_VALUE() (RTC.COUNTER)
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() {}

/* Use the defaults for everything else */
#include_next<FreeRTOSConfig.h>
