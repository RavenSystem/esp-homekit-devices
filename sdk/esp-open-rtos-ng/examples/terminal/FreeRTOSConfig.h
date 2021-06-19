/* Terminal FreeRTOSConfig overrides.

   This is intended as an example of overriding some of the default FreeRTOSConfig settings,
   which are otherwise found in FreeRTOS/Source/include/FreeRTOSConfig.h
*/

/* The serial driver depends on counting semaphores */
#define configUSE_COUNTING_SEMAPHORES 1

/* Use the defaults for everything else */
#include_next<FreeRTOSConfig.h>

