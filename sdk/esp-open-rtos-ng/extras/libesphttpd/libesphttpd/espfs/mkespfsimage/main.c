#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __MINGW32__
#include <io.h>
#endif
#include "espfs.h"
#include "espfsformat.h"

//Heatshrink
#ifdef ESPFS_HEATSHRINK
#include "heatshrink_common.h"
#include "heatshrink_config.h"
#include "heatshrink_encoder.h"
#endif

//Gzip
#ifdef ESPFS_GZIP
// If compiler complains about missing header, try running "sudo apt-get install zlib1g-dev" 
// to install missing package.
#include <zlib.h>
#endif

//Cygwin e.a. needs O_BINARY. Don't miscompile if it's not set.
#ifndef O_BINARY
#define O_BINARY 0
#endif

//Routines to convert host format to the endianness used in the xtensa
short htoxs(short in) {
	char r[2];
	r[0]=in;
	r[1]=in>>8;
	return *((short *)r);
}

int htoxl(int in) {
	unsigned char r[4];
	r[0]=in;
	r[1]=in>>8;
	r[2]=in>>16;
	r[3]=in>>24;
	return *((int *)r);
}

#ifdef ESPFS_HEATSHRINK
size_t compressHeatshrink(char *in, int insize, char *out, int outsize, int level) {
	char *inp=in;
	char *outp=out;
	size_t len;
	int ws[]={5, 6, 8, 11, 13};
	int ls[]={3, 3, 4, 4, 4};
	HSE_poll_res pres;
	HSE_sink_res sres;
	size_t r;
	if (level==-1) level=8;
	level=(level-1)/2; //level is now 0, 1, 2, 3, 4
	heatshrink_encoder *enc=heatshrink_encoder_alloc(ws[level], ls[level]);
	if (enc==NULL) {
		perror("allocating mem for heatshrink");
		exit(1);
	}
	//Save encoder parms as first byte
	*outp=(ws[level]<<4)|ls[level];
	outp++; outsize--;

	r=1;
	do {
		if (insize>0) {
			sres=heatshrink_encoder_sink(enc, inp, insize, &len);
			if (sres!=HSER_SINK_OK) break;
			inp+=len; insize-=len;
			if (insize==0) heatshrink_encoder_finish(enc);
		}
		do {
			pres=heatshrink_encoder_poll(enc, outp, outsize, &len);
			if (pres!=HSER_POLL_MORE && pres!=HSER_POLL_EMPTY) break;
			outp+=len; outsize-=len;
			r+=len;
		} while (pres==HSER_POLL_MORE);
	} while (insize!=0);

	if (insize!=0) {
		fprintf(stderr, "Heatshrink: Bug? insize is still %d. sres=%d pres=%d\n", insize, sres, pres);
		exit(1);
	}

	heatshrink_encoder_free(enc);
	return r;
}
#endif

#ifdef ESPFS_GZIP
size_t compressGzip(char *in, int insize, char *out, int outsize, int level) {
	z_stream stream;
	int zresult;

	stream.zalloc = Z_NULL;
	stream.zfree  = Z_NULL;
	stream.opaque = Z_NULL;
	stream.next_in = in;
	stream.avail_in = insize;
	stream.next_out = out;
	stream.avail_out = outsize;
	// 31 -> 15 window bits + 16 for gzip
	zresult = deflateInit2 (&stream, level, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY);
	if (zresult != Z_OK) {
		fprintf(stderr, "DeflateInit2 failed with code %d\n", zresult);
		exit(1);
	}

	zresult = deflate(&stream, Z_FINISH);
	if (zresult != Z_STREAM_END) {
		fprintf(stderr, "Deflate failed with code %d\n", zresult);
		exit(1);
	}

	zresult = deflateEnd(&stream);
	if (zresult != Z_OK) {
		fprintf(stderr, "DeflateEnd failed with code %d\n", zresult);
		exit(1);
	}

	return stream.total_out;
}

char **gzipExtensions = NULL;

int shouldCompressGzip(char *name) {
	char *ext = name + strlen(name);
	while (*ext != '.') {
		ext--;
		if (ext < name) {
			// no dot in file name -> no extension -> nothing to match against
			return 0;
		}
	}
	ext++;

	int i = 0;
	while (gzipExtensions[i] != NULL) {
		if (strcmp(ext,gzipExtensions[i]) == 0) {
			return 1;
		}
		i++;
	}

	return 0;
}

int parseGzipExtensions(char *input) {
	char *token;
	char *extList = input;
	int count = 2; // one for first element, second for terminator

	// count elements
	while (*extList != 0) {
		if (*extList == ',') count++;
		extList++;
	}

	// split string
	extList = input;
	gzipExtensions = malloc(count * sizeof(char*));
	count = 0;
	token = strtok(extList, ",");
	while (token) {
		gzipExtensions[count++] = token;
		token = strtok(NULL, ",");
	}
	// terminate list
	gzipExtensions[count] = NULL;

	return 1;
}
#endif

int handleFile(int f, char *name, int compression, int level, char **compName) {
	char *fdat, *cdat;
	off_t size, csize;
	EspFsHeader h;
	int nameLen;
	int8_t flags = 0;
	size=lseek(f, 0, SEEK_END);
	fdat=malloc(size);
	lseek(f, 0, SEEK_SET);
	read(f, fdat, size);
	

#ifdef ESPFS_GZIP
	if (shouldCompressGzip(name)) {
		csize = size*3;
		if (csize<100) // gzip has some headers that do not fit when trying to compress small files
			csize = 100; // enlarge buffer if this is the case
		cdat=malloc(csize);
		csize=compressGzip(fdat, size, cdat, csize, level);
		compression = COMPRESS_NONE;
		flags = FLAG_GZIP;
	} else
#endif
	if (compression==COMPRESS_NONE) {
		csize=size;
		cdat=fdat;
#ifdef ESPFS_HEATSHRINK
	} else if (compression==COMPRESS_HEATSHRINK) {
		cdat=malloc(size*2);
		csize=compressHeatshrink(fdat, size, cdat, size*2, level);
#endif
	} else {
		fprintf(stderr, "Unknown compression - %d\n", compression);
		exit(1);
	}

	if (csize>size) {
		//Compressing enbiggened this file. Revert to uncompressed store.
		compression=COMPRESS_NONE;
		csize=size;
		cdat=fdat;
		flags=0;
	}

	//Fill header data
	h.magic=('E'<<0)+('S'<<8)+('f'<<16)+('s'<<24);
	h.flags=flags;
	h.compression=compression;
	h.nameLen=nameLen=strlen(name)+1;
	if (h.nameLen&3) h.nameLen+=4-(h.nameLen&3); //Round to next 32bit boundary
	h.nameLen=htoxs(h.nameLen);
	h.fileLenComp=htoxl(csize);
	h.fileLenDecomp=htoxl(size);
	
	write(1, &h, sizeof(EspFsHeader));
	write(1, name, nameLen);
	while (nameLen&3) {
		write(1, "\000", 1);
		nameLen++;
	}
	write(1, cdat, csize);
	//Pad out to 32bit boundary
	while (csize&3) {
		write(1, "\000", 1);
		csize++;
	}
	free(fdat);

	if (compName != NULL) {
		if (h.compression==COMPRESS_HEATSHRINK) {
			*compName = "heatshrink";
		} else if (h.compression==COMPRESS_NONE) {
			if (h.flags & FLAG_GZIP) {
				*compName = "gzip";
			} else {
				*compName = "none";
			}
		} else {
			*compName = "unknown";
		}
	}
	return size ? (csize*100)/size : 100;
}

//Write final dummy header with FLAG_LASTFILE set.
void finishArchive() {
	EspFsHeader h;
	h.magic=('E'<<0)+('S'<<8)+('f'<<16)+('s'<<24);
	h.flags=FLAG_LASTFILE;
	h.compression=COMPRESS_NONE;
	h.nameLen=htoxs(0);
	h.fileLenComp=htoxl(0);
	h.fileLenDecomp=htoxl(0);
	write(1, &h, sizeof(EspFsHeader));
}

int main(int argc, char **argv) {
	int f, x;
	char fileName[1024];
	char *realName;
	struct stat statBuf;
	int serr;
	int rate;
	int err=0;
	int compType;  //default compression type - heatshrink
	int compLvl=-1;

#ifdef __MINGW32__
	setmode(fileno(stdout), O_BINARY);
#endif
#ifdef ESPFS_HEATSHRINK
	compType = COMPRESS_HEATSHRINK;
#else
	compType = COMPRESS_NONE;
#endif

	for (x=1; x<argc; x++) {
		if (strcmp(argv[x], "-c")==0 && argc>=x-2) {
			compType=atoi(argv[x+1]);
			x++;
		} else if (strcmp(argv[x], "-l")==0 && argc>=x-2) {
			compLvl=atoi(argv[x+1]);
			if (compLvl<1 || compLvl>9) err=1;
			x++;
#ifdef ESPFS_GZIP
		} else if (strcmp(argv[x], "-g")==0 && argc>=x-2) {
			if (!parseGzipExtensions(argv[x+1])) err=1;
			x++;
#endif
		} else {
			err=1;
		}
	}

#ifdef ESPFS_GZIP
	if (gzipExtensions == NULL) {
		parseGzipExtensions(strdup("html,css,js,svg"));
	}
#endif

	if (err) {
		fprintf(stderr, "%s - Program to create espfs images\n", argv[0]);
		fprintf(stderr, "Usage: \nfind | %s [-c compressor] [-l compression_level] ", argv[0]);
#ifdef ESPFS_GZIP
		fprintf(stderr, "[-g gzipped_extensions] ");
#endif
		fprintf(stderr, "> out.espfs\n");
		fprintf(stderr, "Compressors:\n");
#ifdef ESPFS_HEATSHRINK
		fprintf(stderr, "0 - None\n1 - Heatshrink(default)\n");
#else
		fprintf(stderr, "0 - None(default)\n");
#endif
		fprintf(stderr, "\nCompression level: 1 is worst but low RAM usage, higher is better compression \nbut uses more ram on decompression. -1 = compressors default.\n");
#ifdef ESPFS_GZIP
		fprintf(stderr, "\nGzipped extensions: list of comma separated, case sensitive file extensions \nthat will be gzipped. Defaults to 'html,css,js'\n");
#endif
		exit(0);
	}

	while(fgets(fileName, sizeof(fileName), stdin)) {
		//Kill off '\n' at the end
		fileName[strlen(fileName)-1]=0;
		//Only include files
		serr=stat(fileName, &statBuf);
		if ((serr==0) && S_ISREG(statBuf.st_mode)) {
			//Strip off './' or '/' madness.
			realName=fileName;
			if (fileName[0]=='.') realName++;
			if (realName[0]=='/') realName++;
			f=open(fileName, O_RDONLY|O_BINARY);
			if (f>0) {
				char *compName = "unknown";
				rate=handleFile(f, realName, compType, compLvl, &compName);
				fprintf(stderr, "%s (%d%%, %s)\n", realName, rate, compName);
				close(f);
			} else {
				perror(fileName);
			}
		} else {
			if (serr!=0) {
				perror(fileName);
			}
		}
	}
	finishArchive();
	return 0;
}

