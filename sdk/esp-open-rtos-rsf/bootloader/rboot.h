/* rboot header overrides

   This "wrapper" header contains default values for building rboot
   on/for esp-open-rtos. It gets included both when building the
   bootloader and when building extras/rboot-ota support. It includes
   the default bootloader/rboot/rboot.h header via the gcc
   include_next mechanism.
*/
#ifndef __RBOOT_H__

// Big flash support is required for esp-open-rtos (we use 8Mbit
// "slots" only.)
#define BOOT_BIG_FLASH

// enable 2 way communication between
// rBoot and the user app via the esp rtc data area
#define BOOT_RTC_ENABLED

// Call 'main' rboot.h to pick up defaults for other parameters
#include_next "rboot.h"

#endif

