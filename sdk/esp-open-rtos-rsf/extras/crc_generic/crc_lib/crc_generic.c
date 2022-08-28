/**
 * CRC GENERIC LIBRARY.
 *
 * Copyright (c) 2016 Zaltora (https://github.com/Zaltora)
 *
 * BSD Licensed as described in the file LICENSE
 */

#include "crc_generic.h"

#if (CRC_DEBUG == 1)
#define debug(fmt, ...) printf("%s: " fmt "\n", "CRC", ## __VA_ARGS__)
#else
#define debug(fmt, ...)
#endif

#define _CRC_SET_ALGO_SRC(type) \
static type _##type##_reflect(type crc, int bitnum)\
{\
    type i, j=1, crcout=0;\
    for (i=(type)1<<(bitnum-1); i; i>>=1)\
    {\
        if (crc & i) crcout|=j;\
        j<<= 1;\
    }\
    return (crcout);\
}\
void type##_generate_table(config_##type* crcdata, type* tab, crc_16 len)\
{\
	if (crcdata->order&7)\
		return;\
    crc_16 i,j;\
    type bit, crc;\
    for (i=0; i<len; i++)\
    {\
        crc=(type)i;\
        if (crcdata->refin) crc=_##type##_reflect(crc, 8);\
        crc<<= crcdata->order-8;\
        for (j=0; j<8; j++){\
            bit = crc & crcdata->private.crchighbit;\
            crc<<= 1;\
            if (bit) crc^= crcdata->polynom;\
        }\
        if (crcdata->refin) crc = _##type##_reflect(crc, crcdata->order);\
        crc&= crcdata->private.crcmask;\
        tab[i] = crc;\
    }\
}\
static type type##_bit_by_bit_fast_upd(config_##type* crcdata, crc_8* p, crc_16 len)\
{\
    type j, c, bit;\
    type crc = crcdata->private.seed_memory ;\
    while (len--)\
    {\
        c = (type)*p++;\
        if (crcdata->refin)\
            c = _##type##_reflect(c, 8);\
        for (j=0x80; j; j>>=1)\
        {\
            bit = crc & crcdata->private.crchighbit;\
            crc<<= 1;\
            if (c & j) bit^= crcdata->private.crchighbit;\
            if (bit) crc^= crcdata->polynom;\
        }\
    }\
    crcdata->private.seed_memory = crc;\
    if (crcdata->refout) crc=_##type##_reflect(crc, crcdata->order);\
    crc^= crcdata->crcxor;\
    crc&= crcdata->private.crcmask;\
    return(crc);\
}\
static type type##_bit_by_bit_fast(config_##type* crcdata, crc_8* p, crc_16 len)\
{\
    debug("ALGO: bit by bit fast.");\
    crcdata->private.seed_memory = crcdata->private.crcinit_direct;\
    return type##_bit_by_bit_fast_upd(crcdata, p, len);\
}\
static type type##_bit_by_bit_upd(config_##type* crcdata, crc_8* p, crc_16 len)\
{\
    crc_8 i;\
    type j, c, bit;\
    type crc = crcdata->private.seed_memory;\
    while (len--)\
    {\
        c = (type)*p++;\
        if (crcdata->refin)\
            c = _##type##_reflect(c, 8);\
        for (j=0x80; j; j>>=1)\
        {\
            bit = crc & crcdata->private.crchighbit;\
            crc<<= 1;\
            if (c & j) crc|= 1;\
            if (bit) crc^= crcdata->polynom;\
        }\
    }\
    crcdata->private.seed_memory = crc;\
    for (i=0; i<crcdata->order; i++)\
    {\
        bit = crc & crcdata->private.crchighbit;\
        crc<<= 1;\
        if (bit) crc^= crcdata->polynom;\
    }\
    if (crcdata->refout) crc=_##type##_reflect(crc, crcdata->order);\
    crc^= crcdata->crcxor;\
    crc&= crcdata->private.crcmask;\
    return(crc);\
}\
static type type##_bit_by_bit(config_##type* crcdata, crc_8* p, crc_16 len)\
{\
    debug("ALGO: bit by bit.");\
    crcdata->private.seed_memory = crcdata->private.crcinit_nondirect;\
    return type##_bit_by_bit_upd(crcdata, p, len);\
}\
static type type##_table_upd(config_##type* crcdata, crc_8* p, crc_16 len)\
{\
    crc_8 size = ((crcdata->order-1)/8+1)*8;\
    type crc = crcdata->private.seed_memory;\
    if (!crcdata->refin)\
        while (len--) crc = ((crc << 8) | *p++) ^ crcdata->private.tab[(crc >> (size-8))  & 0xff];\
    else\
        while (len--) crc = ((crc >> 8) | (*p++ << (size-8))) ^ crcdata->private.tab[crc & 0xff];\
    crcdata->private.seed_memory= crc ;\
    if (!crcdata->refin)\
        while (++len < size/8) crc = (crc << 8) ^ crcdata->private.tab[(crc >> (size-8)) & 0xff];\
    else\
        while (++len < size/8) crc = (crc >> 8) ^ crcdata->private.tab[crc & 0xff];\
    if (crcdata->refout^crcdata->refin) crc = _##type##_reflect(crc, crcdata->order);\
    crc>>= size-crcdata->order;\
    crc^= crcdata->crcxor;\
    crc&= crcdata->private.crcmask;\
    return(crc);\
}\
static type type##_table (config_##type* crcdata, crc_8* p, crc_16 len)\
{\
    debug("ALGO: table.");\
    crcdata->private.seed_memory = crcdata->private.crcinit_nondirect;\
    if (crcdata->refin) crcdata->private.seed_memory = _##type##_reflect(crcdata->private.seed_memory, crcdata->order);\
    return type##_table_upd(crcdata, p, len);\
}\
static type type##_table_fast_upd(config_##type* crcdata, crc_8* p, crc_16 len)\
{\
    crc_8 size = ((crcdata->order-1)/8+1)*8;\
    type crc = crcdata->private.seed_memory;\
    if (!crcdata->refin)\
        while (len--) { crc = (crc << 8) ^ crcdata->private.tab[((crc >> (size-8)) & 0xff) ^ *p++]; }\
    else\
        while (len--) { crc = (crc >> 8) ^ crcdata->private.tab[(crc & 0xff) ^ *p++];  }\
    crcdata->private.seed_memory= crc ;\
    if (crcdata->refout^crcdata->refin) crc = _##type##_reflect(crc, crcdata->order);\
    crc>>= size-crcdata->order;\
    crc^= crcdata->crcxor;\
    crc&= crcdata->private.crcmask;\
    return(crc);\
}\
static type type##_table_fast(config_##type* crcdata, crc_8* p, crc_16 len)\
{\
    debug("ALGO: table fast.");\
    crcdata->private.seed_memory = crcdata->private.crcinit_direct;\
    if (crcdata->refin) crcdata->private.seed_memory = _##type##_reflect(crcdata->private.seed_memory, crcdata->order);\
    return type##_table_fast_upd(crcdata, p, len);\
}\
void type##_generic_init(config_##type* crcdata, type polynom, type order, type crcinit,\
        type crcxor, crc_8 direct, crc_8 refin, crc_8 refout)\
{\
    crc_16 i;\
    type bit, crc;\
    crcdata->crcinit = crcinit;\
    crcdata->crcxor = crcxor;\
    crcdata->order = order;\
    crcdata->direct = direct;\
    crcdata->polynom = polynom;\
    crcdata->refin = refin;\
    crcdata->refout = refout;\
/* Use default algorithm crc */\
    crcdata->private.crc_compute = (type(*)(config_##type*,crc_8*,crc_16))type##_bit_by_bit_fast;\
    crcdata->private.crc_update = (type(*)(config_##type*,crc_8*,crc_16))type##_bit_by_bit_fast_upd;\
/* Compute mask data */\
    crcdata->private.crcmask = ((((type)1<<(crcdata->order-1))-1)<<1)|1;\
    crcdata->private.crchighbit = (type)1<<(crcdata->order-1);\
/* Do some check */\
    if (crcdata->order < 1 || crcdata->order > sizeof(crcdata->private.crcmask)*8)\
    {\
        debug("ERROR, invalid order, it must be between 1..%u.",sizeof(type)*8);\
        return;\
    }\
    if (crcdata->polynom != (crcdata->polynom & crcdata->private.crcmask))\
    {\
        debug("ERROR, invalid polynom.");\
        return;\
    }\
    if (crcdata->crcinit != (crcdata->crcinit & crcdata->private.crcmask))\
    {\
        debug("ERROR, invalid crcinit.");\
        return;\
    }\
    if (crcdata->crcxor != (crcdata->crcxor & crcdata->private.crcmask))\
    {\
        debug("ERROR, invalid crcxor.");\
        return;\
    }\
    /* compute missing initial CRC value */\
    if (!crcdata->direct)\
    {\
        crcdata->private.crcinit_nondirect = crcdata->crcinit;\
        crc = crcdata->crcinit;\
        for (i=0; i<crcdata->order; i++)\
        {\
            bit = crc & crcdata->private.crchighbit;\
            crc<<= 1;\
            if (bit) crc^= crcdata->polynom;\
        }\
        crc&= crcdata->private.crcmask;\
        crcdata->private.crcinit_direct = crc;\
    }\
    else\
    {\
        crcdata->private.crcinit_direct = crcdata->crcinit;\
        crc = crcdata->crcinit;\
        for (i=0; i<crcdata->order; i++)\
        {\
            bit = crc & 1;\
            if (bit) crc^= crcdata->polynom;\
            crc >>= 1;\
            if (bit) crc|= crcdata->private.crchighbit;\
        }\
        crcdata->private.crcinit_nondirect = crc;\
    }\
}\
void type##_generic_select_algo(config_##type* crcdata, const type* tab, crc_16 len, crc_algo algo)\
{\
    if (tab == NULL)\
    {\
        switch (algo)\
        {\
        case CRC_BIT_TO_BIT:\
            crcdata->private.crc_compute = (type(*)(config_##type*,crc_8*,crc_16))type##_bit_by_bit;\
            crcdata->private.crc_update = (type(*)(config_##type*,crc_8*,crc_16))type##_bit_by_bit_upd;\
            break;\
        case CRC_BIT_TO_BIT_FAST:\
            crcdata->private.crc_compute = (type(*)(config_##type*,crc_8*,crc_16))type##_bit_by_bit_fast;\
            crcdata->private.crc_update = (type(*)(config_##type*,crc_8*,crc_16))type##_bit_by_bit_fast_upd;\
            break;\
        default:\
            debug("ERROR, invalid algo setting. Keep last selection");\
            break;\
        }\
    }\
    else\
    {\
        /*Save location pointer */\
        if (len > 256) return;\
	    crcdata->private.tab = tab;\
        switch (algo)\
        {\
        case CRC_TABLE:\
            crcdata->private.crc_compute = (type(*)(config_##type*,crc_8*,crc_16))type##_table;\
            crcdata->private.crc_update = (type(*)(config_##type*,crc_8*,crc_16))type##_table_upd;\
            break;\
        case CRC_TABLE_FAST:\
            crcdata->private.crc_compute = (type(*)(config_##type*,crc_8*,crc_16))type##_table_fast;\
            crcdata->private.crc_update = (type(*)(config_##type*,crc_8*,crc_16))type##_table_fast_upd;\
            break;\
        default:\
        debug("ERROR, invalid algo setting. Keep last selection");\
        break;\
        }\
    }\
}\
type type##_generic_compute(config_##type* crcdata, crc_8* p, crc_16 len)\
{\
    return crcdata->private.crc_compute(crcdata, p,len);\
}\
type type##_generic_update(config_##type* crcdata, crc_8* p, crc_16 len)\
{\
    return crcdata->private.crc_update(crcdata, p,len);\
}\

/*
 * Create all functions for all size variable usage
 */
#if CRC_1BYTE_SUPPORT == 1
_CRC_SET_ALGO_SRC(crc_8)
#endif

#if CRC_2BYTE_SUPPORT == 1
_CRC_SET_ALGO_SRC(crc_16)
#endif

#if CRC_4BYTE_SUPPORT == 1
_CRC_SET_ALGO_SRC(crc_32)
#endif

#if CRC_8BYTE_SUPPORT == 1
_CRC_SET_ALGO_SRC(crc_64)
#endif
