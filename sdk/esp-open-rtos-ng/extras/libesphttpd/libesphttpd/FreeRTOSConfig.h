/* FreeRTOSConfig overrides.

   This is intended as an example of overriding some of the default FreeRTOSConfig settings,
   which are otherwise found in FreeRTOS/Source/include/FreeRTOSConfig.h
*/

#define configUSE_RECURSIVE_MUTEXES     1

/* Use the defaults for everything else */
#include_next<FreeRTOSConfig.h>

