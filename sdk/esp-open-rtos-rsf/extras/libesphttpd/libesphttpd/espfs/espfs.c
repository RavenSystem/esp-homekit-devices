/*
This is a simple read-only implementation of a file system. It uses a block of data coming from the
mkespfsimg tool, and can use that block to do abstracted operations on the files that are in there.
It's written for use with httpd, but doesn't need to be used as such.
*/

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */


//These routines can also be tested by comping them in with the espfstest tool. This
//simplifies debugging, but needs some slightly different headers. The #ifdef takes
//care of that.

// #include <c_types.h>
typedef unsigned int        uint32;
#include <string.h>
#include <espressif/user_interface.h>
#include <espressif/spi_flash.h>

#ifdef __ets__
//esp build
#else
//Test build
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#endif

#include "espfsformat.h"
#include <libesphttpd/espfs.h>

#ifdef ESPFS_HEATSHRINK
#include "heatshrink_config_custom.h"
#include "heatshrink_decoder.h"
#endif

#ifdef RBOOT_OTA
#include "rboot-api.h"
#endif

static char* espFsData = NULL;


struct EspFsFile {
	EspFsHeader *header;
	char decompressor;
	int32_t posDecomp;
	char *posStart;
	char *posComp;
	void *decompData;
};

/*
Available locations, at least in my flash, with boundaries partially guessed. This
is using 0.9.1/0.9.2 SDK on a not-too-new module.
0x00000 (0x10000): Code/data (RAM data?)
0x10000 (0x02000): Gets erased by something?
0x12000 (0x2E000): Free (filled with zeroes) (parts used by ESPCloud and maybe SSL)
0x40000 (0x20000): Code/data (ROM data?)
0x60000 (0x1C000): Free
0x7c000 (0x04000): Param store
0x80000 - end of flash

Accessing the flash through the mem emulation at 0x40200000 is a bit hairy: All accesses
*must* be aligned 32-bit accesses. Reading a short, byte or unaligned word will result in
a memory exception, crashing the program.
*/

#ifndef ESP32
#define FLASH_BASE_ADDR 0x40200000
#else
//ESP32 IRAM addresses start at 0x40000000, but the IRAM segment actually is mapped
//starting from SPI address 0x40000.
#define FLASH_BASE_ADDR 0x40040000
#endif

#ifdef RBOOT_OTA
EspFsInitResult espFsInit(void *_flashAddress) {
	rboot_config bootconf = rboot_get_config();
	void *flashAddress = (void*)_flashAddress + bootconf.roms[bootconf.current_rom] - 0x2000;
#else
EspFsInitResult espFsInit(void *flashAddress) {
#endif
	if((uint32_t)flashAddress > 0x40000000) {
		flashAddress = (void*)((uint32_t)flashAddress-FLASH_BASE_ADDR);
	}

	// base address must be aligned to 4 bytes
	if (((int)flashAddress & 3) != 0) {
		return ESPFS_INIT_RESULT_BAD_ALIGN;
	}

	// check if there is valid header at address
	EspFsHeader testHeader;
	sdk_spi_flash_read((uint32)flashAddress, (uint32*)&testHeader, sizeof(EspFsHeader));
	if (testHeader.magic != ESPFS_MAGIC) {
		return ESPFS_INIT_RESULT_NO_IMAGE;
	}

	espFsData = (char *)flashAddress;
	return ESPFS_INIT_RESULT_OK;
}

//Copies len bytes over from dst to src, but does it using *only*
//aligned 32-bit reads. Yes, it's no too optimized but it's short and sweet and it works.

//ToDo: perhaps memcpy also does unaligned accesses?
#ifdef __ets__
void readFlashUnaligned(char *dst, char *src, int len) {
	uint8_t src_offset = ((uint32_t)src) & 3;
	uint32_t src_address = ((uint32_t)src) - src_offset;

	uint32_t tmp_buf[len/4 + 2];
	sdk_spi_flash_read((uint32)src_address, (uint32*)tmp_buf, len+src_offset);
	memcpy(dst, ((uint8_t*)tmp_buf)+src_offset, len);
}
#else
#define readFlashUnaligned memcpy
#endif

// Returns flags of opened file.
int espFsFlags(EspFsFile *fh) {
	if (fh == NULL) {
		printf("File handle not ready\n");
		return -1;
	}

	int8_t flags;
	readFlashUnaligned((char*)&flags, (char*)&fh->header->flags, 1);
	return (int)flags;
}

//Open a file and return a pointer to the file desc struct.
EspFsFile *espFsOpen(char *fileName) {
	if (espFsData == NULL) {
		printf("Call espFsInit first!\n");
		return NULL;
	}
	char *p=espFsData;
	char *hpos;
	char namebuf[256];
	EspFsHeader h;
	EspFsFile *r;
	//Strip initial slashes
	while(fileName[0]=='/') fileName++;
	//Go find that file!
	while(1) {
		hpos=p;
		//Grab the next file header.
		sdk_spi_flash_read((uint32)p, (uint32*)&h, sizeof(EspFsHeader));

		if (h.magic!=ESPFS_MAGIC) {
			printf("Magic mismatch. EspFS image broken.\n");
			return NULL;
		}
		if (h.flags&FLAG_LASTFILE) {
			printf("End of image.\n");
			return NULL;
		}
		//Grab the name of the file.
		p+=sizeof(EspFsHeader); 
		sdk_spi_flash_read((uint32)p, (uint32*)&namebuf, sizeof(namebuf));
//		printf("Found file '%s'. Namelen=%x fileLenComp=%x, compr=%d flags=%d\n", 
//				namebuf, (unsigned int)h.nameLen, (unsigned int)h.fileLenComp, h.compression, h.flags);
		if (strcmp(namebuf, fileName)==0) {
			//Yay, this is the file we need!
			p+=h.nameLen; //Skip to content.
			r=(EspFsFile *)malloc(sizeof(EspFsFile)); //Alloc file desc mem
//			printf("Alloc %p\n", r);
			if (r==NULL) return NULL;
			r->header=(EspFsHeader *)hpos;
			r->decompressor=h.compression;
			r->posComp=p;
			r->posStart=p;
			r->posDecomp=0;
			if (h.compression==COMPRESS_NONE) {
				r->decompData=NULL;
#ifdef ESPFS_HEATSHRINK
			} else if (h.compression==COMPRESS_HEATSHRINK) {
				//File is compressed with Heatshrink.
				char parm;
				heatshrink_decoder *dec;
				//Decoder params are stored in 1st byte.
				readFlashUnaligned(&parm, r->posComp, 1);
				r->posComp++;
				printf("Heatshrink compressed file; decode parms = %x\n", parm);
				dec=heatshrink_decoder_alloc(16, (parm>>4)&0xf, parm&0xf);
				r->decompData=dec;
#endif
			} else {
				printf("Invalid compression: %d\n", h.compression);
				free(r);
				return NULL;
			}
			return r;
		}
		//We don't need this file. Skip name and file
		p+=h.nameLen+h.fileLenComp;
		if ((int)p&3) p+=4-((int)p&3); //align to next 32bit val
	}
}

//Read len bytes from the given file into buff. Returns the actual amount of bytes read.
int espFsRead(EspFsFile *fh, char *buff, int len) {
	int flen;
#ifdef ESPFS_HEATSHRINK
	int fdlen;
#endif
	if (fh==NULL) return 0;
		
	readFlashUnaligned((char*)&flen, (char*)&fh->header->fileLenComp, 4);
	//Cache file length.
	//Do stuff depending on the way the file is compressed.
	if (fh->decompressor==COMPRESS_NONE) {
		int toRead;
		toRead=flen-(fh->posComp-fh->posStart);
		if (len>toRead) len=toRead;
//		printf("Reading %d bytes from %x\n", len, (unsigned int)fh->posComp);
		readFlashUnaligned(buff, fh->posComp, len);
		fh->posDecomp+=len;
		fh->posComp+=len;
//		printf("Done reading %d bytes, pos=%x\n", len, fh->posComp);
		return len;
#ifdef ESPFS_HEATSHRINK
	} else if (fh->decompressor==COMPRESS_HEATSHRINK) {
		int decoded=0;
		size_t elen, rlen;
		char ebuff[16];
		heatshrink_decoder *dec=(heatshrink_decoder *)fh->decompData;
//		printf("Alloc %p\n", dec);
		if (dec == NULL) {
			return 0;
		}
		readFlashUnaligned((char*)&fdlen, (char*)&fh->header->fileLenDecomp, 4);
		if (fh->posDecomp == fdlen) {
			return 0;
		}

		// We must ensure that whole file is decompressed and written to output buffer.
		// This means even when there is no input data (elen==0) try to poll decoder until
		// posDecomp equals decompressed file length

		while(decoded<len) {
			//Feed data into the decompressor
			//ToDo: Check ret val of heatshrink fns for errors
			elen=flen-(fh->posComp - fh->posStart);
			if (elen>0) {
				readFlashUnaligned(ebuff, fh->posComp, 16);
				heatshrink_decoder_sink(dec, (uint8_t *)ebuff, (elen>16)?16:elen, &rlen);
				fh->posComp+=rlen;
			}
			//Grab decompressed data and put into buff
			heatshrink_decoder_poll(dec, (uint8_t *)buff, len-decoded, &rlen);
			fh->posDecomp+=rlen;
			buff+=rlen;
			decoded+=rlen;

//			printf("Elen %d rlen %d d %d pd %ld fdl %d\n",elen,rlen,decoded, fh->posDecomp, fdlen);

			if (elen == 0) {
				if (fh->posDecomp == fdlen) {
//					printf("Decoder finish\n");
					heatshrink_decoder_finish(dec);
				}
				return decoded;
			}
		}
		return len;
#endif
	}
	return 0;
}

//Close the file.
void espFsClose(EspFsFile *fh) {
	if (fh==NULL) return;
#ifdef ESPFS_HEATSHRINK
	if (fh->decompressor==COMPRESS_HEATSHRINK) {
		heatshrink_decoder *dec=(heatshrink_decoder *)fh->decompData;
		if (dec)
			heatshrink_decoder_free(dec);
//		printf("Freed %p\n", dec);
	}
#endif
//	printf("Freed %p\n", fh);
	free(fh);
}



