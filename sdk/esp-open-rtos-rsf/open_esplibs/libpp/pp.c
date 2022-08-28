/* Recreated Espressif libpp pp.o contents.

   Copyright (C) 2015 Espressif Systems. Derived from MIT Licensed SDK libraries.
   BSD Licensed as described in the file LICENSE
*/
#include "open_esplibs.h"
#include "stdlib.h"
#include "stdint.h"

/*
 * This replaces a dynamic allocation, a call to zalloc(), from within a
 * critical section in the ppTask. The allocation aquired the malloc lock and
 * doing so withing a critical section is not safe because it might preempt
 * another task which is not possible from within a critical section. The data
 * is written to the rtc memory, and was then freed. The freeing has been
 * patched to be a nop.
 */
static const uint32_t pp_zeros[8];
void *_ppz20(size_t n)
{
    return &pp_zeros;
}

#if OPEN_LIBPP_PP
// The contents of this file are only built if OPEN_LIBPHY_PHY_CHIP_SLEEP is set to true


#endif /* OPEN_LIBPP_PP */
