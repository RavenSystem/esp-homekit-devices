/* esp/slc_regs.h
 *
 * ESP8266 SLC functions
 */

#ifndef _ESP_SLC_H
#define _ESP_SLC_H

#include "esp/slc_regs.h"

/* Memory layout for DMA transfer descriptors. */

struct SLCDescriptor
{
	uint32_t flags;
	uint32_t buf_ptr;
	uint32_t next_link_ptr;
};

#define SLC_DESCRIPTOR_FLAGS_BLOCKSIZE_M 0x00000fff
#define SLC_DESCRIPTOR_FLAGS_BLOCKSIZE_S 0
#define SLC_DESCRIPTOR_FLAGS_DATA_LENGTH_M 0x00000fff
#define SLC_DESCRIPTOR_FLAGS_DATA_LENGTH_S 12
#define SLC_DESCRIPTOR_FLAGS_SUB_SOF BIT(29)
#define SLC_DESCRIPTOR_FLAGS_EOF BIT(30)
#define SLC_DESCRIPTOR_FLAGS_OWNER BIT(31)

#define SLC_DESCRIPTOR_FLAGS(blocksize,datalen,sub_sof,eof,owner) ( \
	VAL2FIELD_M(SLC_DESCRIPTOR_FLAGS_BLOCKSIZE,blocksize)| \
	VAL2FIELD_M(SLC_DESCRIPTOR_FLAGS_DATA_LENGTH,datalen)| \
	((sub_sof)?SLC_DESCRIPTOR_FLAGS_SUB_SOF:0)| \
	((eof)?SLC_DESCRIPTOR_FLAGS_EOF:0)| \
	((owner)?SLC_DESCRIPTOR_FLAGS_OWNER:0) \
) 


#endif /* _ESP_SLC_REGS_H */
