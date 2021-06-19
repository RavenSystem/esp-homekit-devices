/* Recreated Espressif libpp wdev.o contents.

   Copyright (C) 2015 Espressif Systems. Derived from MIT Licensed SDK libraries.
   BSD Licensed as described in the file LICENSE
*/
#include "open_esplibs.h"
#if OPEN_LIBPP_WDEV
// The contents of this file are only built if OPEN_LIBPP_WDEV is set to true

// Note the SDK allocated 8000 bytes for TX buffers that appears to be
// unused. Rather TX buffers appear to be allocated by upper layers. The
// location of this areas is at wDevCtrl + 0x2190. The SDK has been modified
// to allocate only one word (4 bytes) per buffer on initialization and even
// these seem unused but to be sure avoid the first 20 bytes. So there are
// 7980 bytes free starting at wDevCtrl + 0x21a4.

#endif /* OPEN_LIBPP_WDEV */
