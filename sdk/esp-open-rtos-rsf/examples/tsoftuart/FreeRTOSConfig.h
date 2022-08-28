#define configUSE_TRACE_FACILITY 1
#define configGENERATE_RUN_TIME_STATS 1
#define portGET_RUN_TIME_COUNTER_VALUE() (RTC.COUNTER)
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() {}

/* Use the defaults for everything else */
#include_next<FreeRTOSConfig.h>
