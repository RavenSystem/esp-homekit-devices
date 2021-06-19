/**
 * CRC GENERIC LIBRARY.
 *
 * Copyright (c) 2016 Zaltora (https://github.com/Zaltora)
 *
 * BSD Licensed as described in the file LICENSE
 */

#ifndef CRCROUTINES_H_
#define CRCROUTINES_H_

#include "crc_config.h"
#if CRC_1BYTE_SUPPORT == 1
#include "crc_7_lookup_table.h"
#include "crc_8_lookup_table.h"
#endif
#if CRC_2BYTE_SUPPORT == 1
#include "crc_16_lookup_table.h"
#endif
#if CRC_4BYTE_SUPPORT == 1
#include "crc_24_lookup_table.h"
#include "crc_32_lookup_table.h"
#endif
#if CRC_8BYTE_SUPPORT == 1
#include "crc_64_lookup_table.h"
#endif

typedef enum
{
    CRC_BIT_TO_BIT = 0,
    CRC_BIT_TO_BIT_FAST,
    CRC_TABLE,
    CRC_TABLE_FAST
} crc_algo;

#define _CRC_STRUCT(type)  config_##type {\
    type polynom;\
    type crcinit;\
    type crcxor;\
    crc_8 order;\
    crc_8 direct;\
    crc_8 refin ;\
    crc_8 refout;\
    struct {\
        type crcmask;\
        type crchighbit;\
        type crcinit_direct;\
        type crcinit_nondirect;\
        type seed_memory;\
        type (*crc_compute)(struct config_##type*, crc_8*,crc_16);\
        type (*crc_update)(struct config_##type*, crc_8*,crc_16);\
        const type* tab;\
    } private;\
} config_##type;

#define _CRC_PROTOTYPE(type)\
/**
 * Initialize crc configuration
 * @param crcdata Pointer to crc descriptor
 * @param polynom Is the CRC polynom without leading '1' bit
 * @param order Is the CRC polynom order, counted without the leading '1' bit
 * @param crcinit Is the initial CRC value belonging to that algorithm
 * @param crcxor Is the final XOR value
 * @param direct Specifies the kind of algorithm: 1=direct, no augmented zero bits
 * @param refin Specifies if a data byte is reflected before processing (UART) or not
 * @param refout Specifies if the CRC will be reflected before XOR
 */\
void type##_generic_init(config_##type* crcdata, type polynom, type order, type crcinit,\
        type crcxor, crc_8 direct, crc_8 refin, crc_8 refout);\
/**
 * Generate table
 * @param crcdata Pointer to crc descriptor
 * @param tab Pointer to  array
 * @param len Array size
 */\
void type##_generate_table(config_##type* crcdata, type* tab, crc_16 len);\
/**
 * Select algorithm
 * Warning can only generate lookup tab with polynom orders of 8, 16, 24 or 32.
 * @param crcdata Pointer to crc descriptor
 * @param tab Is the CRC lookup table pointer
 * @param len Array size
 * @param cst Specifies if the tab is constant (will be generated if not)
 */\
void type##_generic_select_algo(config_##type* crcdata, const type* tab, crc_16 len, crc_algo algo);\
/**
 * Compute crc
 * @param crcdata Pointer to crc descriptor
 * @param *p Pointer to data
 * @param len Specifies the size of input data (in byte)
 */\
type type##_generic_compute(config_##type* crcdata, crc_8* p, crc_16 len);\
/**
 * Update current crc
 * @param crcdata Pointer to crc descriptor
 * @param *p Pointer to data
 * @param len Specifies the size of input data (in byte)
 */\
type type##_generic_update(config_##type* crcdata, crc_8* p, crc_16 len);

#if CRC_1BYTE_SUPPORT
typedef struct _CRC_STRUCT(crc_8)
_CRC_PROTOTYPE(crc_8)
#endif

#if CRC_2BYTE_SUPPORT
typedef struct _CRC_STRUCT(crc_16)
_CRC_PROTOTYPE(crc_16)
#endif

#if CRC_4BYTE_SUPPORT
typedef struct _CRC_STRUCT(crc_32)
_CRC_PROTOTYPE(crc_32)
#endif

#if CRC_8BYTE_SUPPORT
typedef struct _CRC_STRUCT(crc_64)
_CRC_PROTOTYPE(crc_64)
#endif

//More Details abouts crc parameter
/*
   polynom: This parameter is the poly. This is a binary value that
   should be specified as a hexadecimal number. The top bit of the
   poly should be omitted. For example, if the poly is 10110, you
   should specify 06. An important aspect of this parameter is that it
   represents the unreflected poly; the bottom bit of this parameter
   is always the LSB of the divisor during the division regardless of
   whether the algorithm being modelled is reflected.

   crcinit: This parameter specifies the initial value of the register
   when the algorithm starts. This is the value that is to be assigned
   to the register in the direct table algorithm. In the table
   algorithm, we may think of the register always commencing with the
   value zero, and this value being XORed into the register after the
   N'th bit iteration. This parameter should be specified as a
   hexadecimal number.

   refin: This is a boolean parameter. If it is FALSE, input bytes are
   processed with bit 7 being treated as the most significant bit
   (MSB) and bit 0 being treated as the least significant bit. If this
   parameter is FALSE, each byte is reflected before being processed.

   refout: This is a boolean parameter. If it is set to FALSE, the
   final value in the register is fed into the XOROUT stage directly,
   otherwise, if this parameter is TRUE, the final register value is
   reflected first.

   crcxor: This is an W-bit value that should be specified as a
   hexadecimal number. It is XORed to the final register value (after
   the REFOUT) stage before the value is returned as the official
   checksum.

   check value: This field is not strictly part of the definition, and, in
   the event of an inconsistency between this field and the other
   field, the other fields take precedence. This field is a check
   value that can be used as a weak validator of implementations of
   the algorithm. The field contains the checksum obtained when the
   ASCII string "123456789" is fed through the specified algorithm
   (i.e. 313233... (hexadecimal)).
 */

#endif /* CRCROUTINES_H_ */
