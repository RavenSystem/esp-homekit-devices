/* Blink FreeRTOSConfig overrides.

   This is intended as an example of overriding some of the default FreeRTOSConfig settings,
   which are otherwise found in FreeRTOS/Source/include/FreeRTOSConfig.h
*/

/* We sleep a lot, so cooperative multitasking is fine. */
#define configUSE_PREEMPTION 0

/* Blink doesn't really need a lot of stack space! */
#define configMINIMAL_STACK_SIZE 128

/* Use the defaults for everything else */
#include_next<FreeRTOSConfig.h>

