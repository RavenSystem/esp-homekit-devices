/* Recreated Espressif libmain xtensa_context.o contents.

   Copyright (C) 2015 Espressif Systems. Derived from MIT Licensed SDK libraries.
   BSD Licensed as described in the file LICENSE
*/
#include "open_esplibs.h"
#if OPEN_LIBMAIN_XTENSA_CONTEXT
// The contents of this file are only built if OPEN_LIBMAIN_XTENSA_CONTEXT is set to true

        .section .iram1.text, "ax", @progbits

        .balign 4
        .global sdk__xt_context_save
        .type   sdk__xt_context_save, @function
sdk__xt_context_save:

        s32i    a2, sp, 20
        s32i    a3, sp, 24
        s32i    a4, sp, 28
        s32i    a5, sp, 32
        s32i    a6, sp, 36
        s32i    a7, sp, 40
        s32i    a8, sp, 44
        s32i    a9, sp, 48
        s32i    a10, sp, 52
        s32i    a11, sp, 56
        rsr     a3, sar
        s32i    a3, sp, 76
        ret

        .balign 4
        .global sdk__xt_context_restore
        .type   sdk__xt_context_restore, @function
sdk__xt_context_restore:
        l32i    a3, sp, 76
        l32i    a2, sp, 20
        wsr     a3, sar
        l32i    a3, sp, 24
        l32i    a4, sp, 28
        l32i    a5, sp, 32
        l32i    a6, sp, 36
        l32i    a7, sp, 40
        l32i    a8, sp, 44
        l32i    a9, sp, 48
        l32i    a10, sp, 52
        l32i    a11, sp, 56
        l32i    a12, sp, 60
        l32i    a13, sp, 64
        ret

#endif /* OPEN_LIBMAIN_XTENSA_CONTEXT */
